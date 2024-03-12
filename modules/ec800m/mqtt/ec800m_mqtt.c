/**
 * @file ec800mqtt.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-12-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "ec800m_mqtt.h"
#include "nrfx_gpiote.h"

#define NRF_LOG_MODULE_NAME ec800mqtt
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static ec800m_t*     _ec800m;
ec800m_mqtt_status_t mqtt_status;
ec800m_mqtt_t        mqtt_config;

int mqtt_task_publish(ec800m_t* dev, ec800m_mqtt_task_t task, void* param)
{
    ec800m_task_t task_cb = {
        .type  = EC800_MQTT,
        .task  = task,
        .param = param,
    };
    return xQueueSend(dev->task_queue, &task_cb, EC800M_IPC_MIN_TICK);
}

int ec800m_mqtt_conf(ec800m_mqtt_t* cfg)
{
    if (cfg->host == NULL || cfg->port == NULL || cfg->clientid == NULL) {
        NRF_LOG_INFO("host and port of mqtt should be configure.");
        return -EINVAL;
    }
    memcpy(&mqtt_config, cfg, sizeof(ec800m_mqtt_t));
    return EOK;
}

int ec800m_mqtt_connect(void)
{
    mqtt_task_publish(_ec800m, EC800M_TASK_MQTT_OPEN, NULL);
    return ec800m_wait_sync(_ec800m, 10000);
}

int ec800m_mqtt_pub(char* topic, void* payload, uint32_t len)
{
    int result = EOK;
    if (topic == NULL || payload == NULL) {
        NRF_LOG_INFO("topic and payload should be specified.");
        result = -EINVAL;
        goto __exit;
    }
    char* tmp_payload = (char*)malloc(len + 1);
    if (tmp_payload == NULL) {
        NRF_LOG_ERROR("tmpbuf for mqtt payload create fail");
        result = -ENOMEM;
        goto __exit;
    }
    strcpy(tmp_payload, payload);
    tmp_payload[len] = '\0';

    if (mqtt_status == EC800M_MQTT_CONN) {
        ec800m_mqtt_data_t _mqtt_data = {
            .topic   = topic,
            .payload = tmp_payload,
            .len     = len,
        };
        mqtt_task_publish(_ec800m, EC800M_TASK_MQTT_PUB, &_mqtt_data);
        result = ec800m_wait_sync(_ec800m, 5000);
    } else {
        NRF_LOG_WARNING("mqtt has not connected, try to reconnect", mqtt_status);
        mqtt_task_publish(_ec800m, EC800M_TASK_MQTT_CHECK, NULL);
        result = ec800m_wait_sync(_ec800m, 10000);
    }
__exit:

    free(tmp_payload);
    return result;
}

int ec800m_mqtt_sub(char* subtopic)
{
    if (subtopic == NULL) {
        NRF_LOG_INFO("subtopic should be specified.");
        return -EINVAL;
    }
    if (mqtt_status == EC800M_MQTT_CONN) {

        mqtt_task_publish(_ec800m, EC800M_TASK_MQTT_SUB, subtopic);
    } else {
        NRF_LOG_WARNING("mqtt has not connected, try to reconnect", mqtt_status);
        mqtt_task_publish(_ec800m, EC800M_TASK_MQTT_CHECK, NULL);
        return ec800m_wait_sync(_ec800m, 10000);
    }
    return EOK;
}

static void ec800m_mqtt_init_handle(ec800m_t* dev)
{
    _ec800m     = dev;
    mqtt_status = EC800M_MQTT_IDLE;
    extern const struct at_urc mqtt_urc_table[];
    at_obj_set_urc_table(_ec800m->client, mqtt_urc_table, 5);
}

static void ec800m_mqtt_task_handle(ec800m_task_t* task_cb, ec800m_t* dev)
{
    xSemaphoreTake(dev->lock, portMAX_DELAY);
    if (task_cb->task == EC800M_TASK_MQTT_CHECK) {
        mqtt_status = EC800M_MQTT_IDLE;
        dev->err    = at_cmd_exec(dev->client, AT_CMD_MQTT_STATUS, NULL);
    }
    if (task_cb->task == EC800M_TASK_MQTT_OPEN) {
        if (mqtt_status == EC800M_MQTT_IDLE) {
            dev->err = at_cmd_exec(dev->client, AT_CMD_MQTT_CLOSE, NULL);
        } else {
            if (mqtt_config.keepalive) {
                dev->err = at_cmd_exec(dev->client, AT_CMD_MQTT_CONF_ALIVE, NULL, mqtt_config.keepalive);
            }
            dev->err = at_cmd_exec(dev->client, AT_CMD_MQTT_REL, NULL, mqtt_config.host, mqtt_config.port);
        }
    }
    if (task_cb->task == EC800M_TASK_MQTT_CLOSE) {
        dev->err = at_cmd_exec(dev->client, AT_CMD_MQTT_CLOSE, NULL);
    }
    if (task_cb->task == EC800M_TASK_MQTT_CONNECT) {
        if (mqtt_status == EC800M_MQTT_OPEN) {
            char cmd_para[30] = {0};
            if (mqtt_config.username && mqtt_config.password) {
                sprintf(cmd_para, "%s,%s,%s", mqtt_config.clientid, mqtt_config.username, mqtt_config.password);
            } else {
                sprintf(cmd_para, "%s", mqtt_config.clientid);
            }
            dev->err = at_cmd_exec(dev->client, AT_CMD_MQTT_CONNECT, NULL, cmd_para);
        }
    }
    if (task_cb->task == EC800M_TASK_MQTT_PUB) {
        ec800m_mqtt_data_t* data = (ec800m_mqtt_data_t*)task_cb->param;
        at_set_end_sign('>');
        dev->err = at_cmd_exec(dev->client, AT_CMD_MQTT_PUBLISH, NULL, data->topic, data->len);
        at_client_send(data->payload, data->len);
        at_set_end_sign(0);
        ec800m_post_sync(dev);
    }
    if (task_cb->task == EC800M_TASK_MQTT_SUB) {
        char* sub = (char*)task_cb->param;
        dev->err  = at_cmd_exec(_ec800m->client, AT_CMD_MQTT_SUBSCRIBE, sub);
    }
}

static void ec800m_mqtt_err_handle(ec800m_task_t* task_cb, ec800m_t* dev)
{
    /**
     * @todo err handle
     *
     */
    xSemaphoreGive(dev->lock);
}

ec800m_task_group_t ec800m_mqtt_task_group = {
    .id          = EC800_MQTT,
    .init        = ec800m_mqtt_init_handle,
    .task_handle = ec800m_mqtt_task_handle,
    .err_handle  = ec800m_mqtt_err_handle,
};
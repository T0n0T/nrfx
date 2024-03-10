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

#include "ec800m.h"
#include "ec800m_mqtt.h"
#include "nrfx_gpiote.h"

#define NRF_LOG_MODULE_NAME ec800mqtt
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

EventGroupHandle_t   mqtt_event;
ec800m_mqtt_status_t mqtt_status;
ec800m_mqtt_t        mqtt_config;

int mqtt_task_publish(ec800m_mqtt_task_t task, uint32_t timeout)
{
    ec800m_task_t task_cb = {
        .type    = EC800_MQTT,
        .task    = task,
        .param   = NULL,
        .timeout = timeout,
    };
    return xQueueSend(ec800m.task_queue, &task_cb, 300);
}

int mqtt_task_wait_response()
{
    EventBits_t rec = xEventGroupWaitBits(mqtt_event,
                                          EC800M_MQTT_SUCCESS | EC800M_MQTT_TIMEOUT | EC800M_MQTT_ERROR,
                                          pdTRUE,
                                          pdFALSE,
                                          portMAX_DELAY);
    if (rec & EC800M_MQTT_SUCCESS) {
        return EOK;
    }
    if (rec & EC800M_MQTT_ERROR) {
        return ERROR;
    }
    if (rec & EC800M_MQTT_TIMEOUT) {
        return ETIMEOUT;
    }
    return ERROR;
}

int ec800m_mqtt_conf(ec800m_mqtt_t* cfg)
{
    char prase_str[20] = {0};
    if (cfg->host == NULL || cfg->port == NULL || cfg->clientid == NULL) {
        NRF_LOG_INFO("host and port of mqtt should be configure.");
        return -EINVAL;
    }
    memcpy(&mqtt_config, cfg, sizeof(ec800m_mqtt_t));
    return EOK;
}

int ec800m_mqtt_connect(void)
{
    mqtt_task_publish(EC800M_TASK_MQTT_OPEN, 1000);
    return mqtt_task_wait_response();
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
        nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
        at_set_end_sign('>');
        at_cmd_exec(ec800m.client, AT_CMD_MQTT_PUBLISH, topic, len);
        at_client_send(tmp_payload, len);
        at_set_end_sign(0);
        nrf_gpio_pin_write(ec800m.wakeup_pin, 1);
    } else {
        NRF_LOG_WARNING("mqtt has not connected, try to reconnect", mqtt_status);
        mqtt_task_publish(EC800M_TASK_MQTT_CHECK, 10000);
        result = mqtt_task_wait_response();
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
        nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
        at_cmd_exec(ec800m.client, AT_CMD_MQTT_SUBSCRIBE, subtopic);
        nrf_gpio_pin_write(ec800m.wakeup_pin, 1);
    } else {
        NRF_LOG_WARNING("mqtt has not connected, try to reconnect", mqtt_status);
        mqtt_task_publish(EC800M_TASK_MQTT_CHECK, 10000);
        return mqtt_task_wait_response();
    }
    return EOK;
}

static void ec800m_mqtt_init_handle(void)
{
    mqtt_event  = xEventGroupCreate();
    mqtt_status = EC800M_MQTT_IDLE;
    extern const struct at_urc mqtt_urc_table[];
    at_obj_set_urc_table(ec800m.client, mqtt_urc_table, 5);
}

static void ec800m_mqtt_task_handle(int task, void* param)
{
    if (task == EC800M_TASK_MQTT_CHECK) {
        mqtt_status = EC800M_MQTT_IDLE;
        at_cmd_exec(ec800m.client, AT_CMD_MQTT_STATUS);
    }
    if (task == EC800M_TASK_MQTT_OPEN) {
        if (mqtt_status == EC800M_MQTT_IDLE) {
            at_cmd_exec(ec800m.client, AT_CMD_MQTT_CLOSE);
        } else {
            if (mqtt_config.keepalive) {
                at_cmd_exec(ec800m.client, AT_CMD_MQTT_CONF_ALIVE, mqtt_config.keepalive);
            }
            at_cmd_exec(ec800m.client, AT_CMD_MQTT_REL, mqtt_config.host, mqtt_config.port);
        }
    }
    if (task == EC800M_TASK_MQTT_CLOSE) {
        at_cmd_exec(ec800m.client, AT_CMD_MQTT_CLOSE);
    }
    if (task == EC800M_TASK_MQTT_CONNECT) {
        if (mqtt_status == EC800M_MQTT_OPEN) {
            char cmd_para[30] = {0};
            if (mqtt_config.username && mqtt_config.password) {
                sprintf(cmd_para, "%s,%s,%s", mqtt_config.clientid, mqtt_config.username, mqtt_config.password);
            } else {
                sprintf(cmd_para, "%s", mqtt_config.clientid);
            }
            at_cmd_exec(ec800m.client, AT_CMD_MQTT_CONNECT, cmd_para);
        }
    }
}

static void ec800m_mqtt_timeout_handle(int task, void* param)
{
    xEventGroupSetBits(mqtt_event, EC800M_MQTT_TIMEOUT);
}

ec800m_task_group_t ec800m_mqtt_task_group = {
    .id             = EC800_MQTT,
    .init           = ec800m_mqtt_init_handle,
    .task_handle    = ec800m_mqtt_task_handle,
    .timeout_handle = ec800m_mqtt_timeout_handle,
};
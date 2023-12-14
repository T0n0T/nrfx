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

#define NRF_LOG_MODULE_NAME ec800mqtt
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

ec800m_mqtt_t mqtt_config;

void ec800m_mqtt_task_handle(ec800m_task_t task)
{
    switch (task) {
        case EC800M_TASK_MQTT_CHECK:
            ec800m.status = EC800M_IDLE;
            at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_STATUS);
            xTimerStart(ec800m.timer, 0);
            break;
        case EC800M_TASK_MQTT_RELEASE:
            if (ec800m.status == EC800M_IDLE) {
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CLOSE);
                vTaskDelay(500);
                ec800m.status = EC800M_MQTT_CLOSE;
                if (mqtt_config.keepalive) {
                    at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CONF_ALIVE, mqtt_config.keepalive);
                }
                if (ec800m.status == EC800M_MQTT_CLOSE) {
                    at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_REL, mqtt_config.host, mqtt_config.port);
                }
            } else {
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_REL, mqtt_config.host, mqtt_config.port);
            }
            break;
        case EC800M_TASK_MQTT_CONNECT:
            if (ec800m.status == EC800M_MQTT_OPEN) {
                char cmd_para[30] = {0};
                if (mqtt_config.username && mqtt_config.password) {
                    sprintf(cmd_para, "%s,%s,%s", mqtt_config.clientid, mqtt_config.username, mqtt_config.password);
                } else {
                    sprintf(cmd_para, "%s", mqtt_config.clientid);
                }
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CONNECT, cmd_para);
            } else {
                NRF_LOG_WARNING("mqtt has not released");
            }
            break;

        default:
            break;
    }
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
    ec800m_task_t task = EC800M_TASK_MQTT_RELEASE;
    return xQueueSend(ec800m.task_queue, &task, 300);
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

    if (ec800m.status == EC800M_MQTT_CONN) {
        nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
        at_set_end_sign('>');
        at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_PUBLISH, topic, len);
        at_client_send(tmp_payload, len);
        at_set_end_sign(0);

        nrf_gpio_pin_write(ec800m.wakeup_pin, 1);
    } else {
        NRF_LOG_WARNING("mqtt has not connected, try to reconnect", ec800m.status);
        ec800m_task_t task = EC800M_TASK_MQTT_CHECK;
        xQueueSend(ec800m.task_queue, &task, 300);
        result = -ERROR;
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
    if (ec800m.status == EC800M_MQTT_CONN) {
        nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
        at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_SUBSCRIBE, subtopic);
        nrf_gpio_pin_write(ec800m.wakeup_pin, 1);
    } else {
        NRF_LOG_WARNING("mqtt has not connected, try to reconnect", ec800m.status);
        ec800m_task_t task = EC800M_TASK_MQTT_CHECK;
        xQueueSend(ec800m.task_queue, &task, 300);
        return -ERROR;
    }
    return EOK;
}
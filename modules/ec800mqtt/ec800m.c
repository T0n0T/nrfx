/**
 * @file ec800m.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-26
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "ec800m.h"
#include "nrfx_gpiote.h"

#define NRF_LOG_MODULE_NAME ec800mqtt
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static char                send_buf[EC800M_BUF_LEN];
extern const struct at_urc urc_table[];
ec800m_t                   ec800m;
static TaskHandle_t        task_handle = NULL;

/**
 * @brief
 *
 * @param dev
 * @param at_cmd_id
 * @param prase_here
 * @return int
 */

/**
 * @brief execute cmd with id
 *
 * @param dev get at_client instance
 * @param prase_buf if need to prase after the func,it should support by user
 * @param at_cmd_id located cmd from list
 * @param ... args used to combine with cmd
 * @return int
 */
int at_cmd_exec(at_client_t dev, char* prase_buf, at_cmd_desc_t at_cmd_id, ...)
{
    va_list       args;
    int           result        = EOK;
    const char*   recv_line_buf = 0;
    at_cmd_t      cmd           = (at_cmd_t)AT_CMD(at_cmd_id);
    at_response_t resp          = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, cmd->timeout);
    if (resp == NULL) {
        NRF_LOG_ERROR("No memory for response structure!");
        return -ENOMEM;
    }
    memset(send_buf, 0, sizeof(send_buf));

    va_start(args, at_cmd_id);
    vsprintf(send_buf, cmd->cmd_expr, args);
    va_end(args);

    result = at_obj_exec_cmd(dev, resp, send_buf);
    if (result < 0) {
        goto __exit;
    }

    if (prase_buf) {
        if ((recv_line_buf = at_resp_get_line_by_kw(resp, cmd->resp_keyword)) == NULL) {
            NRF_LOG_ERROR("AT client execute [%s] failed!", cmd->desc);
            goto __exit;
        }
        strncpy(prase_buf, recv_line_buf, strlen(recv_line_buf));
    }

__exit:
    if (resp) {
        at_delete_resp(resp);
    }

    return result;
}

void ec800m_task_handle(ec800m_task_t* task)
{
    switch (*task) {
        case EC800M_TASK_CHECK:
            ec800m.status = EC800M_IDLE;
            at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_STATUS);
            xTimerStart(ec800m.timer, 0);
            break;
        case EC800M_TASK_MQTT_RELEASE:
            if (ec800m.status == EC800M_IDLE) {
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CLOSE);
                vTaskDelay(500);
                if (ec800m.mqtt.keepalive) {
                    at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CONF_ALIVE, ec800m.mqtt.keepalive);
                }
                if (ec800m.status == EC800M_MQTT_CLOSE) {
                    at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_REL, ec800m.mqtt.host, ec800m.mqtt.port);
                }
            } else {
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_REL, ec800m.mqtt.host, ec800m.mqtt.port);
            }
            break;
        case EC800M_TASK_MQTT_CONNECT:
            if (ec800m.status == EC800M_MQTT_OPEN) {
                char cmd_para[30] = {0};
                if (ec800m.mqtt.username && ec800m.mqtt.password) {
                    sprintf(cmd_para, "%s,%s,%s", ec800m.mqtt.clientid, ec800m.mqtt.username, ec800m.mqtt.password);
                } else {
                    sprintf(cmd_para, "%s", ec800m.mqtt.clientid);
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

void ec800m_task(void* p)
{
    int result = EOK;
    nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
    result = at_client_obj_wait_connect(ec800m.client, 1000);
    if (result < 0) {
        NRF_LOG_ERROR("at client connect failed.");
        goto __exit;
    }

    /** reset ec800m */
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_RESET);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m reset failed.");
        goto __exit;
    }
    vTaskDelay(500);

    /** turn off cmd echo */
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_ECHO_OFF);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m echo set off failed.");
        goto __exit;
    }

    /** check simcard with checking pin */
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_CHECK_PIN);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m simcard detect failed.");
        goto __exit;
    }

    /** check signal */
    char* parse_str = malloc(20);
    result          = at_cmd_exec(ec800m.client, parse_str, AT_CMD_CHECK_SIGNAL);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m check signal failed.");
        goto __exit;
    } else {
        int rssi = 0, err_rate = 0;
        sscanf(parse_str, "+CSQ: %d,%d", &rssi, &err_rate);
        if (rssi != 99 && rssi) {
            ec800m.rssi = rssi;
        } else {
            NRF_LOG_WARNING("ec800m signal strength get wrong.");
        }
    }

    /** configure gnss*/
    memset(parse_str, 0, sizeof(parse_str));
    result = at_cmd_exec(ec800m.client, parse_str, AT_CMD_GNSS_STATUS);
    if (result < 0) {
        goto __exit;
    } else {
        int gnss_status = -1;
        sscanf(parse_str, "+QGPS: %d", &gnss_status);
        switch (gnss_status) {
            case 0:
                result = at_cmd_exec(ec800m.client, NULL, AT_CMD_GNSS_OPEN);
                if (result < 0) {
                    goto __exit;
                }
                break;
            case 1:
                NRF_LOG_INFO("gnss has already open!");
                break;
            default:
                NRF_LOG_ERROR("Command [AT+QGPS?] fail!");
                result = -ERROR;
                goto __exit;
        }
    }

    /** only use rmc */
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_GNSS_NMEA_CONF, 2);
    if (result < 0) {
        goto __exit;
    }

    vTaskDelay(1000);
    /** use sleep mode */
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_LOW_POWER);
    if (result < 0) {
        goto __exit;
    }
    nrf_gpio_pin_write(ec800m.wakeup_pin, 1);
    free(parse_str);
    /** set urc */
    at_obj_set_urc_table(ec800m.client, urc_table, 5);
    ec800m.status = EC800M_IDLE;
    ec800m_task_t task_cb;
    while (1) {
        memset(&task_cb, 0, sizeof(ec800m_task_t));
        xQueueReceive(ec800m.task_queue, &task_cb, portMAX_DELAY);
        nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
        ec800m_task_handle(&task_cb);
        nrf_gpio_pin_write(ec800m.wakeup_pin, 1);
        // xTimerStart(ec800m.timer, 0);
    }

__exit:
    vTaskDelete(NULL);
}

int ec800m_mqtt_conf(ec800m_mqtt_t* cfg)
{
    char prase_str[20] = {0};
    if (cfg->host == NULL || cfg->port == NULL || cfg->clientid == NULL) {
        NRF_LOG_INFO("host and port of mqtt should be configure.");
        return -EINVAL;
    }
    memcpy(&ec800m.mqtt, cfg, sizeof(ec800m_mqtt_t));
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
        ec800m_task_t task = EC800M_TASK_CHECK;
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
        ec800m_task_t task = EC800M_TASK_CHECK;
        xQueueSend(ec800m.task_queue, &task, 300);
        return -ERROR;
    }
    return EOK;
}

gps_info_t ec800m_gnss_get(void)
{
    char                   rmc[128] = {0};
    static struct gps_info rmcinfo  = {0};
    at_response_t          resp     = at_create_resp(128, 0, 300);
    nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
    if (at_obj_exec_cmd(ec800m.client, resp, "AT+QGPSGNMEA=\"RMC\"") == EOK) {
        if (at_resp_parse_line_args_by_kw(resp, "+QGPSGNMEA:", "+QGPSGNMEA:%s", rmc) > 0) {
            if (!gps_rmc_parse(&rmcinfo, rmc)) {
                NRF_LOG_WARNING("gnss info invalid");
            } else {
                NRF_LOG_DEBUG("%d %d %d %d %d %d", rmcinfo.date.year, rmcinfo.date.month, rmcinfo.date.day,
                              rmcinfo.date.hour, rmcinfo.date.minute, rmcinfo.date.second);
            }
        }
    }
    nrf_gpio_pin_write(ec800m.wakeup_pin, 1);
    at_delete_resp(resp);
    return &rmcinfo;
}

void ec800m_timer_cb(TimerHandle_t timer)
{
    switch (ec800m.status) {
        case EC800M_MQTT_CONN:
            break;
        default:
            ec800m.reset_need++;
            ec800m_task_t task = EC800M_TASK_CHECK;
            xQueueSend(ec800m.task_queue, &task, 0);
            if (ec800m.reset_need >= EC800M_RESET_MAX) {
                NRF_LOG_INFO("ec800m timer reset");
                vTaskDelete(task_handle);
                xTaskCreate(ec800m_task,
                            "EC800M",
                            1024,
                            0,
                            3,
                            &task_handle);
                vTaskResume(task_handle);
                ec800m.reset_need = 0;
            }
            break;
    }
}

void ec800m_init(void)
{
    if (at_client_init("EC800MCN", AT_CLIENT_RECV_BUFF_LEN) < 0) {
        NRF_LOG_ERROR("at client ec800m serial init failed.");
        return;
    }

    memset(&ec800m, 0, sizeof(ec800m));
    ec800m.client        = at_client_get(NULL);
    ec800m.pwr_pin       = EC800_PIN_PWR;
    ec800m.wakeup_pin    = EC800_PIN_DTR;
    ec800m.task_queue    = xQueueCreate(10, sizeof(ec800m_task_t));
    ec800m.timer         = xTimerCreate("ec800m_timer", pdMS_TO_TICKS(3000), pdFALSE, (void*)0, ec800m_timer_cb);
    BaseType_t xReturned = xTaskCreate(ec800m_task,
                                       "EC800M",
                                       1024,
                                       0,
                                       3,
                                       &task_handle);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
}

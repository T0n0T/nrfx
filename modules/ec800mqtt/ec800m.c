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

#define NRF_LOG_MODULE_NAME ec800mqtt
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static char                send_buf[EC800M_BUF_LEN];
extern const struct at_urc urc_table[];
ec800m_t                   ec800m;

int ec800m_mqtt_connect(char* host, char* port,
                        char* client_id, char* username, char* password,
                        int   keep_alive,
                        char* sub_topic);

int ec800m_mqtt_pub(char* topic, char* payload);
int ec800m_mqtt_sub(char* sub_topic);

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

    if ((recv_line_buf = at_resp_get_line_by_kw(resp, cmd->resp_keyword)) == NULL) {
        NRF_LOG_ERROR("AT client execute [%s] failed!", cmd->desc);
        goto __exit;
    }

    if (prase_buf) {
        strncpy(prase_buf, recv_line_buf, strlen(recv_line_buf));
        NRF_LOG_DEBUG("prase_buf: %s", prase_buf);
    }

__exit:
    if (resp) {
        at_delete_resp(resp);
    }

    return result;
}

void ec800m_event_handle(EventBits_t uxBits)
{
    switch (uxBits) {
        case EC800M_EVENT_MQTT_RELEASE:
            if (ec800m.status == EC800M_IDLE) {
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CLOSE);
                vTaskDelay(500);
                if (ec800m.status == EC800M_MQTT_CLOSE) {
                    at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_REL, ec800m.mqtt.host, ec800m.mqtt.port);
                }
            } else {
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_REL, ec800m.mqtt.host, ec800m.mqtt.port);
            }
            break;
        case EC800M_EVENT_MQTT_CONNECT:
            if (ec800m.status == EC800M_MQTT_OPEN) {
                char cmd_para[30] = {0};
                if (ec800m.mqtt.user_name && ec800m.mqtt.password) {
                    sprintf(cmd_para, "%s,%s,%s", ec800m.mqtt.client_id, ec800m.mqtt.username, ec800m.mqtt.password);
                } else {
                    sprintf(cmd_para, "%s", ec800m.mqtt.client_id);
                }
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CONNECT, cmd_para);
            } else {
                NRF_LOG_WARNING("mqtt has not released");
            }
            break;
        case EC800M_EVENT_MQTT_ALIVE:
            if (ec800m.status == EC800M_MQTT_CLOSE) {
                if (ec800m.mqtt.keep_alive) {
                    at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CONF_ALIVE, ec800m.mqtt.keep_alive);
                }
            } else {
                NRF_LOG_WARNING("do keepalive configuration only when mqtt closed");
            }
            break;
        case EC800M_EVENT_MQTT_SUBCIRBE:
            if (ec800m.status == EC800M_MQTT_CONN) {
                at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_SUBSCRIBE, ec800m.mqtt.subtopic);
            } else {
                NRF_LOG_WARNING("mqtt has not connected");
            }

        default:
            break;
    }
}

void ec800m_timer_cb(TimerHandle_t timer)
{
    if (ec800m.status == EC800M_MQTT_CONN) {
        /* code */
    }
}

void ec800m_init_task(void* p)
{
    int result = EOK;
    NRF_LOG_DEBUG("Init at client device.");
    result = at_client_init("EC800MCN", AT_CLIENT_RECV_BUFF_LEN);
    if (result < 0) {
        NRF_LOG_ERROR("at client ec800m serial init failed.");
        return;
    }

    memset(&ec800m, 0, sizeof(ec800m));
    ec800m.client     = at_client_get(NULL);
    ec800m.pwr_pin    = EC800_PIN_PWR;
    ec800m.wakeup_pin = EC800_PIN_DTR;
    ec800m.event      = xEventGroupCreate();
    ec800m.timer      = xTimerCreate("ec800m_timer", pdMS_TO_TICKS(1000), pdFALSE, (void*)0, ec800m_timer_cb);

    result = at_client_obj_wait_connect(ec800m.client, 1000);
    if (result < 0) {
        NRF_LOG_ERROR("at client connect failed.");
        goto __exit;
    }

    /** reset ec800m */
    // result = at_cmd_exec(ec800m.client, NULL, AT_CMD_RESET);
    // if (result < 0) {
    //     NRF_LOG_ERROR("ec800m reset failed.");
    //     goto __exit;
    // }
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
    char signal_str[10] = {0};
    result              = at_cmd_exec(ec800m.client, signal_str, AT_CMD_CHECK_SIGNAL);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m check signal failed.");
        goto __exit;
    } else {
        int rssi = 0, err_rate = 0;
        sscanf(signal_str, "+CSQ: %d,%d", &rssi, &err_rate);
        if (rssi != 99 && rssi) {
            ec800m.rssi = rssi;
        } else {
            NRF_LOG_WARNING("ec800m signal strength get wrong.");
        }
    }

    /** set urc */
    at_obj_set_urc_table(ec800m.client, urc_table, 5);
    // ec800m_mqtt_connect("broker.emqx.io", "1883", "gryacawv", NULL, NULL, NULL, "topic/example");
    xEventGroupSetBits(ec800m.event, EC800M_EVENT_MQTT_RELEASE);
    EventBits_t uxBits;
    while (1) {
        uxBits = xEventGroupWaitBits(ec800m.event,
                                     EC800M_EVENT_MQTT_RELEASE |
                                         EC800M_EVENT_MQTT_CONNECT |
                                         EC800M_EVENT_MQTT_ALIVE |
                                         EC800M_EVENT_MQTT_SUBCIRBE |
                                         EC800M_EVENT_MQTT_PUBLISH |
                                         EC800M_EVENT_GNSS_CONF |
                                         EC800M_EVENT_GNSS_OPEN |
                                         EC800M_EVENT_GNSS_FULSH,
                                     pdTRUE, pdFALSE, portMAX_DELAY);
        ec800m_event_handle(uxBits);
        xTimerStart(ec800m.timer, 0);
    }

__exit:
    vTaskDelete(NULL);
}

int ec800m_mqtt_connect(char* host, char* port,
                        char* client_id, char* username, char* password,
                        int   keep_alive,
                        char* sub_topic)
{
    int result = EOK;

    char prase_str[20] = {0};
    if (host == NULL || port == NULL || client_id == NULL) {
        NRF_LOG_INFO("host and port of mqtt should be configure.");
        result = -EINVAL;
        goto __exit;
    }

    /** configure keepalive if needed */
    if (keep_alive) {
        result = at_cmd_exec(ec800m.client, NULL, AT_CMD_MQTT_CONF_ALIVE, keep_alive);
        if (result < 0) {
            goto __exit;
        }
    }

    /** open mqtt client */
    result = at_cmd_exec(ec800m.client, prase_str, AT_CMD_MQTT_REL, host, port);

    /** mqtt connect */
    char cmd_para[30] = {0};
    if (username && password) {
        sprintf(cmd_para, "%s,%s,%s", client_id, username, password);
    } else {
        sprintf(cmd_para, "%s", client_id);
    }
    memset(prase_str, 0, sizeof(prase_str));
    result = at_cmd_exec(ec800m.client, prase_str, AT_CMD_MQTT_CONNECT, cmd_para);
    if (result < 0) {
        goto __exit;
    } else {
    }

    /** check mqtt status */
    vTaskDelay(500);
    memset(prase_str, 0, sizeof(prase_str));
    result = at_cmd_exec(ec800m.client, prase_str, AT_CMD_MQTT_STATUS);
    if (result < 0) {
        goto __exit;
    } else {
        int state = 0;
        sscanf(prase_str, "+QMTCONN: %*d,%d", &state);
        switch (state) {
            case 1:
                NRF_LOG_INFO("mqtt is initializing.");
                result = -EBUSY;
                break;
            case 2:
                NRF_LOG_INFO("mqtt is connecting.");
                result = -EBUSY;
                break;
            case 3:
                NRF_LOG_INFO("mqtt connect success.");
                break;
            case 4:
                NRF_LOG_INFO("mqtt is disconnecting.");
                result = -ERROR;
                break;
            default:
                break;
        }
    }

    /** configure sub topic if needed */
    if (sub_topic) {
        result = ec800m_mqtt_sub(sub_topic);
    }

__exit:
    return result;
}

int ec800m_mqtt_pub(char* topic, char* payload)
{
    int result = EOK;

    char prase_str[10] = {0};
    if (topic == NULL || payload == NULL) {
        NRF_LOG_INFO("topic and payload should be specified.");
        result = -EINVAL;
    }

    result = at_cmd_exec(ec800m.client, prase_str, AT_CMD_MQTT_PUBLISH, topic, payload);
    if (result < 0) {
        goto __exit;
    } else {
    }

__exit:
    return result;
}

int ec800m_mqtt_sub(char* sub_topic)
{
    int result = EOK;

    if (sub_topic == NULL) {
        NRF_LOG_INFO("sub_topic should be specified.");
        result = -EINVAL;
    }
    result =
        if (result < 0)
    {
        goto __exit;
    }
    else
    {
    }

__exit:
    return result;
}

int ec800m_init(void)
{
    static TaskHandle_t task_handle = NULL;

    BaseType_t xReturned = xTaskCreate(ec800m_init_task,
                                       "EC800M",
                                       512,
                                       0,
                                       3,
                                       &task_handle);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
}

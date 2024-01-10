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

#define NRF_LOG_MODULE_NAME ec800
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static char                       send_buf[EC800M_BUF_LEN];
static TaskHandle_t               ec800m_task_handle;
ec800m_t                          ec800m;
extern ec800m_task_group_t        ec800m_mqtt_task_group;
extern ec800m_task_group_t        ec800m_socket_task_group;
static const ec800m_task_group_t* task_groups[] = {
#if EC800M_MQTT_SOFT
    &ec800m_mqtt_task_group,
#else
    &ec800m_socket_task_group,
#endif
};
static void (*timeout)(void);

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

void ec800M_wake_up(void)
{
    int result = EOK;
    do {
        nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
        NRF_LOG_DEBUG("wake up module!!");
        result = at_client_obj_wait_connect(ec800m.client, 10000);
        vTaskDelay(pdMS_TO_TICKS(500));
    } while (result < 0);
}

void ec800m_task(void* p)
{
    int result = EOK;
    // nrf_gpio_pin_write(ec800m.wakeup_pin, 0);
    // result = at_client_obj_wait_connect(ec800m.client, 10000);
    // if (result < 0) {
    //     NRF_LOG_ERROR("at client connect failed.");
    //     goto __exit;
    // }
    ec800M_wake_up();

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
    free(parse_str);

    for (size_t i = 0; i < sizeof(task_groups) / sizeof(ec800m_task_group_t*); i++) {
        task_groups[i]->init();
    }
    ec800m.status = EC800M_IDLE;
    ec800m_task_t task_cb;
    NRF_LOG_DEBUG("ec800m init OK!");
    TickType_t last_time = 0, current_time = 0;
    while (1) {
        memset(&task_cb, 0, sizeof(ec800m_task_t));
        last_time = xTaskGetTickCount();
        xQueueReceive(ec800m.task_queue, &task_cb, portMAX_DELAY);
        current_time = xTaskGetTickCount();
        if (current_time - last_time > pdMS_TO_TICKS(15000)) {
            ec800M_wake_up();
        }

        for (size_t i = 0; i < sizeof(task_groups) / sizeof(ec800m_task_group_t*); i++) {
            if (task_groups[i]->id == task_cb.type) {
                timeout = task_groups[i]->timeout_handle;
                xTimerChangePeriod(ec800m.timer, pdMS_TO_TICKS(task_cb.timeout), 0);
                task_groups[i]->task_handle(task_cb.task, task_cb.param);
                if (task_cb.timeout > 0) {
                    xTimerStart(ec800m.timer, 0);
                }
                break;
            }
        }
    }

__exit:
    NRF_LOG_ERROR("ec800m init fail!");
    vTaskDelete(NULL);
}

gps_info_t ec800m_gnss_get(void)
{
    char                   rmc[128] = {0};
    static struct gps_info rmcinfo  = {0};
    at_response_t          resp     = at_create_resp(128, 0, 300);
    ec800M_wake_up();
    if (at_obj_exec_cmd(ec800m.client, resp, "AT+QGPSGNMEA=\"RMC\"") == EOK) {
        if (at_resp_parse_line_args_by_kw(resp, "+QGPSGNMEA:", "+QGPSGNMEA:%s", rmc) > 0) {
            NRF_LOG_DEBUG("rmc: %s", rmc);
            if (!gps_rmc_parse(&rmcinfo, rmc)) {
                NRF_LOG_WARNING("gnss info invalid");
            } else {
                NRF_LOG_DEBUG("%d %d %d %d %d %d", rmcinfo.date.year, rmcinfo.date.month, rmcinfo.date.day,
                              rmcinfo.date.hour, rmcinfo.date.minute, rmcinfo.date.second);
            }
        }
    }
    at_delete_resp(resp);
    return &rmcinfo;
}

void ec800m_timer_cb(TimerHandle_t timer)
{
    UNUSED_PARAMETER(timer);
    timeout();
}

void ec800m_init(void)
{
    if (at_client_init("EC800MCN", AT_CLIENT_RECV_BUFF_LEN) < 0) {
        NRF_LOG_ERROR("at client ec800m serial init failed.");
        return;
    }

    memset(&ec800m, 0, sizeof(ec800m));
    ec800m.client     = at_client_get(NULL);
    ec800m.pwr_pin    = EC800_PIN_PWR;
    ec800m.wakeup_pin = EC800_PIN_DTR;
    ec800m.task_queue = xQueueCreate(10, sizeof(ec800m_task_t));
    ec800m.timer      = xTimerCreate("ec800m_timer", pdMS_TO_TICKS(3000), pdFALSE, (void*)0, ec800m_timer_cb);

    BaseType_t xReturned = xTaskCreate(ec800m_task,
                                       "EC800M",
                                       512,
                                       NULL,
                                       configMAX_PRIORITIES - 2,
                                       &ec800m_task_handle);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
}

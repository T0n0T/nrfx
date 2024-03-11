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
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static char                       send_buf[EC800M_BUF_LEN];
static char                       recv_buf[EC800M_RECV_BUFF_LEN];
static TaskHandle_t               ec800m_task_handle;
ec800m_t                          ec800m;
extern ec800m_task_group_t        ec800m_comm_task_group;
extern ec800m_task_group_t        ec800m_mqtt_task_group;
extern ec800m_task_group_t        ec800m_socket_task_group;
static const ec800m_task_group_t* task_groups[] = {
#if EC800M_MQTT_SOFT
    &ec800m_mqtt_task_group,
#else
    &ec800m_socket_task_group,
#endif
};

static struct at_response resp = {
    .buf      = recv_buf,
    .buf_size = EC800M_RECV_BUFF_LEN,
};
/**
 * @brief execute cmd with id
 *
 * @param dev get at_client instance
 * @param prase_buf if need to prase after the func,it should support by user
 * @param at_cmd_id located cmd from list
 * @param ... args used to combine with cmd
 * @return int
 */
int at_cmd_exec(at_client_t dev, at_cmd_desc_t at_cmd_id, char* keyword_line, ...)
{
    va_list     args;
    int         result        = EOK;
    const char* recv_line_buf = 0;
    at_cmd_t    cmd           = (at_cmd_t)AT_CMD(at_cmd_id);

    memset(send_buf, 0, sizeof(send_buf));
    resp.line_counts = 0;
    resp.line_num    = cmd->resp_linenum;
    resp.timeout     = cmd->timeout;

    va_start(args, at_cmd_id);
    vsprintf(send_buf, cmd->cmd_expr, args);
    va_end(args);

    result = at_obj_exec_cmd(dev, &resp, send_buf);
    if (result < 0) {
        goto __exit;
    }

    if (keyword_line && cmd->resp_keyword) {
        if ((recv_line_buf = at_resp_get_line_by_kw(&resp, cmd->resp_keyword)) == NULL) {
            NRF_LOG_ERROR("recvline err [%s]!", cmd->desc);
            goto __exit;
        }
        strncpy(keyword_line, recv_line_buf, strlen(recv_line_buf));
    }

__exit:

    return result;
}

void ec800m_wake_up(void)
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
    for (size_t i = 0; i < sizeof(task_groups) / sizeof(ec800m_task_group_t*); i++) {
        task_groups[i]->init();
    }
    TickType_t    last_time = 0, current_time = 0;
    ec800m_task_t task_cb;

    ec800m_power_on();

    ec800m.status = EC800M_POWER_OFF;
    while (1) {
        memset(&task_cb, 0, sizeof(ec800m_task_t));
        last_time = xTaskGetTickCount();
        xQueueReceive(ec800m.task_queue, &task_cb, portMAX_DELAY);
        if (ec800m.status == EC800M_POWER_OFF) {
            continue;
        } else if (ec800m.status == EC800M_POWER_LOW) {
            current_time = xTaskGetTickCount();
            if (current_time - last_time > pdMS_TO_TICKS(10000)) {
                ec800m_wake_up();
            }
        }

        for (size_t i = 0; i < sizeof(task_groups) / sizeof(ec800m_task_group_t*); i++) {
            if (task_groups[i]->id == task_cb.type) {
                task_groups[i]->task_handle(task_cb.task, task_cb.param);
                break;
            }
        }
    }
}

void ec800m_init(void)
{
    if (at_client_init("EC800MCN", EC800M_RECV_BUFF_LEN) < 0) {
        NRF_LOG_ERROR("at client ec800m serial init failed.");
        return;
    }

    memset(&ec800m, 0, sizeof(ec800m));
    ec800m.client     = at_client_get(NULL);
    ec800m.pwr_pin    = EC800_PIN_PWREN;
    ec800m.wakeup_pin = EC800_PIN_DTR;
    ec800m.task_queue = xQueueCreate(10, sizeof(ec800m_task_t));

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

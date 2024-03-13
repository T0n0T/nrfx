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

static char               send_buf[EC800M_BUF_LEN];
static char               recv_buf[EC800M_RECV_BUFF_LEN];
static TaskHandle_t       ec800m_task_handle;
static struct at_response resp = {
    .buf      = recv_buf,
    .buf_size = EC800M_RECV_BUFF_LEN,
};

extern ec800m_task_group_t ec800m_comm_task_group;
extern ec800m_task_group_t ec800m_mqtt_task_group;
extern ec800m_task_group_t ec800m_socket_task_group;
static ec800m_t            ec800m;

static const ec800m_task_group_t* task_groups[] = {
    &ec800m_comm_task_group,
#if EC800M_MQTT_SOFT
    &ec800m_mqtt_task_group,
#else
    &ec800m_socket_task_group,
#endif
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
int at_cmd_exec(at_client_t dev, at_cmd_desc_t at_cmd_id, char** keyword_line, ...)
{
    va_list     args;
    int         result        = EOK;
    const char* recv_line_buf = 0;
    at_cmd_t    cmd           = (at_cmd_t)AT_CMD(at_cmd_id);

    memset(send_buf, 0, sizeof(send_buf));
    resp.line_counts = 0;
    resp.timeout     = cmd->timeout;

    if (keyword_line) {
        resp.line_num = cmd->resp_linenum;
    } else {
        resp.line_num = 0;
    }

    va_start(args, keyword_line);
    vsprintf(send_buf, cmd->cmd_expr, args);
    va_end(args);

    result = at_obj_exec_cmd(dev, &resp, send_buf);
    if (result < 0) {
        goto __exit;
    }

    if (keyword_line && cmd->resp_keyword) {
        if ((recv_line_buf = at_resp_get_line_by_kw(&resp, cmd->resp_keyword)) == NULL) {
            result = -EINVAL;
            goto __exit;
        }

        *keyword_line = (char*)recv_line_buf;
    }

__exit:
    // if (keyword_line) {
    //     NRF_LOG_DEBUG("[%s] recvline [%s]!", cmd->desc, *keyword_line);
    // }

    return result;
}

void ec800m_wake_up(ec800m_t* dev)
{
    int result = EOK;
    do {
        nrf_gpio_pin_write(dev->wakeup_pin, 0);
        NRF_LOG_DEBUG("wake up module!!");
        result = at_client_obj_wait_connect(dev->client, 10000);
        vTaskDelay(pdMS_TO_TICKS(500));
    } while (result < 0);
}

void ec800m_task(void* p)
{
    int           result    = EOK;
    ec800m_t*     dev       = (ec800m_t*)p;
    TickType_t    last_time = 0, current_time = 0;
    ec800m_task_t task_cb;

    dev->status = EC800M_POWER_OFF;

    for (size_t i = 0; i < sizeof(task_groups) / sizeof(ec800m_task_group_t*); i++) {
        task_groups[i]->init(dev);
    }
    while (1) {
        memset(&task_cb, 0, sizeof(ec800m_task_t));
        last_time = xTaskGetTickCount();
        if (xSemaphoreGetMutexHolder(dev->lock) != NULL) {
            NRF_LOG_WARNING("ec800m busy!");
            continue;
        }

        xQueueReceive(dev->task_queue, &task_cb, portMAX_DELAY);
        if (dev->status == EC800M_POWER_OFF && task_cb.type != EC800_COMM) {
            NRF_LOG_DEBUG("task:%d %d", task_cb.type, task_cb.task)
            NRF_LOG_WARNING("ec800m power has turned off!");
            continue;
        } else if (dev->status == EC800M_POWER_LOW) {
            current_time = xTaskGetTickCount();
            if (current_time - last_time > pdMS_TO_TICKS(10000)) {
                ec800m_wake_up(dev);
            }
        }

        for (size_t i = 0; i < sizeof(task_groups) / sizeof(ec800m_task_group_t*); i++) {
            if (task_groups[i]->id == task_cb.type) {
                // dev->err = EOK;
                task_groups[i]->task_handle(&task_cb, dev);
                task_groups[i]->err_handle(&task_cb, dev);
                break;
            }
        }
        if (dev->status == EC800M_POWER_LOW) {
            nrf_gpio_pin_write(dev->wakeup_pin, 1);
        }
    }
}

int _ec800m_wait_sync(ec800m_t* dev, uint32_t timeout)
{
    if (xSemaphoreTake(dev->sync, timeout) != pdTRUE) {
        NRF_LOG_DEBUG("waiting over!")
        return -ETIMEOUT;
    }
    NRF_LOG_DEBUG("waiting success!")
    return dev->err;
}

void _ec800m_post_sync(ec800m_t* dev)
{
    xSemaphoreGive(dev->sync);
}

ec800m_t* ec800m_init(void)
{
    if (at_client_init("EC800MCN", EC800M_RECV_BUFF_LEN) < 0) {
        NRF_LOG_ERROR("at client ec800m serial init failed.");
        return 0;
    }

    memset(&ec800m, 0, sizeof(ec800m));
    ec800m.client            = at_client_get(NULL);
    ec800m.client->user_data = &ec800m;
    ec800m.pwr_pin           = EC800_PIN_PWREN;
    ec800m.wakeup_pin        = EC800_PIN_DTR;
    ec800m.task_queue        = xQueueCreate(10, sizeof(ec800m_task_t));
    ec800m.sync              = xSemaphoreCreateBinary();
    ec800m.lock              = xSemaphoreCreateMutex();

    BaseType_t xReturned = xTaskCreate(ec800m_task,
                                       "EC800M",
                                       512,
                                       &ec800m,
                                       configMAX_PRIORITIES - 2,
                                       &ec800m_task_handle);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
        return 0;
    }
    return &ec800m;
}

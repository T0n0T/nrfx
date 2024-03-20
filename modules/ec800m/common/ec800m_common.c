/**
 * @file ec800m_common.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-03-09
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "ec800m.h"
#include "ec800m_common.h"

#define NRF_LOG_MODULE_NAME ec800comm
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static struct gps_info rmcinfo;
SemaphoreHandle_t      power_sync;

static int comm_task_publish(ec800m_t* dev, ec800m_comm_task_t task, void* param)
{
    ec800m_task_t task_cb = {
        .type  = EC800_COMM,
        .task  = task,
        .param = param,
    };
    return xQueueSend(dev->task_queue, &task_cb, EC800M_IPC_MIN_TICK);
}

void ec800m_power_on(ec800m_t* dev)
{
    comm_task_publish(dev, EC800M_TASK_POWERON, NULL);
    ec800m_wait_sync(dev, portMAX_DELAY);
}

void ec800m_power_off(ec800m_t* dev)
{
    comm_task_publish(dev, EC800M_TASK_POWEROFF, NULL);
}

void ec800m_power_low(ec800m_t* dev)
{
    comm_task_publish(dev, EC800M_TASK_POWERLOW, NULL);
}

void ec800m_signal_check(ec800m_t* dev)
{
    comm_task_publish(dev, EC800M_TASK_SIGNAL_CHECK, NULL);
}

void ec800m_gnss_open(ec800m_t* dev)
{
    comm_task_publish(dev, EC800M_TASK_GNSS_OPEN, NULL);
    ec800m_wait_sync(dev, portMAX_DELAY);
}

gps_info_t ec800m_gnss_get(ec800m_t* dev)
{
    comm_task_publish(dev, EC800M_TASK_GNSS_GET, NULL);
    /** take sync from task_handle */
    if (ec800m_wait_sync(dev, portMAX_DELAY) < 0) {
        NRF_LOG_WARNING("gnss info invalid! err[%d]", dev->err);
        return 0;
    }
    return &rmcinfo;
}

static void ec800m_comm_init_handle(ec800m_t* dev)
{
    nrf_gpio_cfg_output(dev->pwr_pin);
    nrf_gpio_cfg_output(dev->wakeup_pin);
    power_sync = xSemaphoreCreateBinary();
    extern const struct at_urc comm_urc_table[];
    at_obj_set_urc_table(dev->client, comm_urc_table, 2);
}

static void ec800m_comm_task_handle(ec800m_task_t* task_cb, ec800m_t* dev)
{
    // task which need resp should wait inside this func
    xSemaphoreTake(dev->lock, portMAX_DELAY);
    if (task_cb->task == EC800M_TASK_POWERON) {
        int result = EOK;
        int retry  = 5;
        nrf_uart_enable(dev->client->device->p_reg);
        nrf_gpio_pin_write(dev->pwr_pin, 0);
        vTaskDelay(500);
        nrf_gpio_pin_write(dev->pwr_pin, 1);
        /** task sync from urc 'RDY' */
        if (xSemaphoreTake(power_sync, 10000) != pdTRUE) {
            NRF_LOG_ERROR("ec800m hardfault!");
            result = -ETIMEOUT;
            goto __power_on_exit;
        }
        /** reset ec800m */
        result = at_cmd_exec(dev->client, AT_CMD_RESET, NULL);
        if (result < 0) {
            NRF_LOG_ERROR("ec800m reset failed.");
            goto __power_on_exit;
        }
        vTaskDelay(500);

        /** turn off cmd echo */
        result = at_cmd_exec(dev->client, AT_CMD_ECHO_OFF, NULL);
        if (result < 0) {
            NRF_LOG_ERROR("ec800m reset failed.");
            goto __power_on_exit;
        }

        char* keyword_line = NULL;
        do {
            /** check simcard with checking pin */
            vTaskDelay(1000);
            result = at_cmd_exec(dev->client, AT_CMD_CHECK_PIN, &keyword_line);
        } while (result < 0 && retry--);
        if (result < 0) {
            NRF_LOG_ERROR("ec800m simcard detect failed.");
            goto __power_on_exit;
        }
        dev->status = EC800M_POWER_ON;
        NRF_LOG_DEBUG("ec800m init OK!");

    __power_on_exit:
        if (result < 0) {
            dev->status = EC800M_POWER_OFF;
            dev->err    = result;
            NRF_LOG_ERROR("ec800m init failed!");
        }
        ec800m_post_sync(dev);
    }
    if (task_cb->task == EC800M_TASK_POWEROFF) {
        // release ec800m.lock in urc
        dev->err = at_cmd_exec(dev->client, AT_CMD_POWER_DOWN, NULL);
    }
    if (task_cb->task == EC800M_TASK_POWERLOW) {
        dev->err = at_cmd_exec(dev->client, AT_CMD_LOW_POWER, NULL);
        if (dev->err == EOK) {
            dev->status = EC800M_POWER_LOW;
        }
    }
    if (task_cb->task == EC800M_TASK_SIGNAL_CHECK) {
        char* keyword_line = NULL;
        dev->err           = at_cmd_exec(dev->client, AT_CMD_CHECK_SIGNAL, &keyword_line);
        int rssi = 0, err_rate = 0;
        sscanf(keyword_line, "+CSQ: %d,%d", &rssi, &err_rate);
        if (rssi != 99 && rssi) {
            NRF_LOG_INFO("ec800m signal check rssi=%d", rssi);
        } else {
            // NRF_LOG_WARNING("ec800m signal strength get wrong. %d", rssi);
        }
    }
    if (task_cb->task == EC800M_TASK_GNSS_OPEN) {
        int   result       = EOK;
        char* keyword_line = NULL;
        result             = at_cmd_exec(dev->client, AT_CMD_GNSS_STATUS, &keyword_line);
        if (result < 0) {
            goto __gnss_check_exit;
        } else {
            int gnss_status = -1;
            sscanf(keyword_line, "+QGPS: %d", &gnss_status);
            switch (gnss_status) {
                case 0:
                    result = at_cmd_exec(dev->client, AT_CMD_GNSS_OPEN, NULL);
                    break;
                case 1:
                    NRF_LOG_INFO("gnss has already open!");
                    break;
                default:
                    NRF_LOG_ERROR("Command [AT+QGPS?] fail!");
                    result = -ERROR;
                    goto __gnss_check_exit;
            }
        }
        /** configure gnss*/
        result = at_cmd_exec(dev->client, AT_CMD_GNSS_NMEA_CONF, NULL, 2);
    __gnss_check_exit:
        if (result < 0) {
            NRF_LOG_ERROR("ec800m gnss check failed!");
        }
        ec800m_post_sync(dev);
    }
    if (task_cb->task == EC800M_TASK_GNSS_CHECK) {
        char* keyword_line = NULL;
        dev->err           = at_cmd_exec(dev->client, AT_CMD_GNSS_LOC, &keyword_line);
    }
    if (task_cb->task == EC800M_TASK_GNSS_GET) {
        uint8_t retry        = 10;
        char*   keyword_line = NULL;
        dev->err             = at_cmd_exec(dev->client, AT_CMD_GNSS_NMEA_RMC, &keyword_line);
        if (dev->err == EOK) {
            char rmc[128] = {0};
            if (sscanf(keyword_line, "+QGPSGNMEA:%s", rmc) > 0) {
                if (!gps_rmc_parse(&rmcinfo, rmc)) {
                    dev->err = -EINVAL;
                } else {
                    NRF_LOG_INFO("gnss: %d %d %d %d %d %d", rmcinfo.date.year, rmcinfo.date.month, rmcinfo.date.day,
                                 rmcinfo.date.hour, rmcinfo.date.minute, rmcinfo.date.second);
                }
            }
        }
        ec800m_post_sync(dev);
    }
}

static void ec800m_comm_err_handle(ec800m_task_t* task_cb, ec800m_t* dev)
{
    /**
     * @todo err handle
     *
     */
    xSemaphoreGive(dev->lock);
}

ec800m_task_group_t ec800m_comm_task_group = {
    .id          = EC800_COMM,
    .init        = ec800m_comm_init_handle,
    .task_handle = ec800m_comm_task_handle,
    .err_handle  = ec800m_comm_err_handle,
};
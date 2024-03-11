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
#include "nrfx_gpiote.h"

#define NRF_LOG_MODULE_NAME ec800comon
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

int ec800m_power_on(void)
{
    int result = EOK;
    int retry  = 5;
    nrf_gpio_cfg_output(ec800m.pwr_pin);
    nrf_gpio_pin_write(ec800m.pwr_pin, 1);
    vTaskDelay(3000);
    /** reset ec800m */
    result = at_cmd_exec(ec800m.client, AT_CMD_RESET);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m reset failed.");
        goto __exit;
    }
    vTaskDelay(500);

    /** turn off cmd echo */
    result = at_cmd_exec(ec800m.client, AT_CMD_ECHO_OFF);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m echo set off failed.");
        goto __exit;
    }

    do {
        /** check simcard with checking pin */
        result = at_cmd_exec(ec800m.client, AT_CMD_CHECK_PIN);
        vTaskDelay(1000);
    } while (result < 0 && retry--);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m simcard detect failed.");
        goto __exit;
    }
    ec800m.status = EC800M_POWER_ON;
    NRF_LOG_DEBUG("ec800m init OK!");

    return 0;
__exit:
    NRF_LOG_ERROR("ec800m init fail!");
    return -1;
}

void ec800m_power_off(void)
{
    at_cmd_exec(ec800m.client, AT_CMD_POWER_DOWN);
    // todo: urc
    //  nrf_gpio_pin_write(ec800m.pwr_pin, 0);
    //  ec800m.status = EC800M_POWER_OFF;
}

void ec800m_power_low(void)
{
    at_cmd_exec(ec800m.client, AT_CMD_LOW_POWER);
}

int ec800m_check_signal(void)
{
    /** check signal */
    if (at_cmd_exec(ec800m.client, AT_CMD_CHECK_SIGNAL) < 0) {
        NRF_LOG_ERROR("ec800m check signal failed.");
    } else {
        int rssi = 0, err_rate = 0;
        // sscanf(parse_str, "+CSQ: %d,%d", &rssi, &err_rate);
        if (rssi != 99 && rssi) {
            ec800m.rssi = rssi;
        } else {
            NRF_LOG_WARNING("ec800m signal strength get wrong.");
        }
    }
}

int ec800m_gnss_check(void)
{
    /** configure gnss*/
    // at_cmd_exec(ec800m.client, AT_CMD_GNSS_STATUS);
    // if (result < 0) {
    //     goto __exit;
    // } else {
    //     //todo: urc
    //     int gnss_status = -1;
    //     sscanf(parse_str, "+QGPS: %d", &gnss_status);
    //     switch (gnss_status) {
    //         case 0:
    //             result = at_cmd_exec(ec800m.client, AT_CMD_GNSS_OPEN);
    //             if (result < 0) {
    //                 goto __exit;
    //             }
    //             break;
    //         case 1:
    //             NRF_LOG_INFO("gnss has already open!");
    //             break;
    //         default:
    //             NRF_LOG_ERROR("Command [AT+QGPS?] fail!");
    //             result = -ERROR;
    //             goto __exit;
    //     }
    // }
}

int ec800m_gnss_open(void)
{
    /** only use rmc */
    at_cmd_exec(ec800m.client, AT_CMD_GNSS_NMEA_CONF, 2);
}

gps_info_t ec800m_gnss_get(void)
{
    at_cmd_exec(ec800m.client, AT_CMD_GNSS_NMEA_RMC);
    return 0;
}

void ec800m_comm_init_handle(void)
{
}

void ec800m_comm_task_handle(void)
{
    //task which need resp should wait inside this func
}

void ec800m_comm_timeout_handle(void)
{
    //think about give up this func,because timer expiring event can be solve in the func above
}

ec800m_task_group_t ec800m_comm_task_group = {
    .id             = EC800_COMM,
    .init           = ec800m_comm_init_handle,
    .task_handle    = ec800m_comm_task_handle,
    // .timeout_handle = ec800m_comm_timeout_handle,
};
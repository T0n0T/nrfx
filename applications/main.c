/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-29     supperthomas first version
 *
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>
#include <app.h>
#include "app_error.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ble_app_log.h"

#include "SEGGER_RTT.h"
static int log_init(void);

#define DK_BOARD_LED_1 LED1

int main(void)
{
    log_init();
    APP_ERROR_CHECK(nrf_pwr_mgmt_init());
    rt_pin_mode(DK_BOARD_LED_1, PIN_MODE_OUTPUT);
    mission_init();
    while (1) {
        if (NRF_LOG_PROCESS() == false) {
            nrf_pwr_mgmt_run();
        }
        // SEGGER_RTT_printf(0, "Hello s-Thread!\r\n");
        rt_pin_write(DK_BOARD_LED_1, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(DK_BOARD_LED_1, PIN_LOW);
        rt_thread_mdelay(500);
    }
    return RT_EOK;
}

static int log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    // nrf_log_backend_rt_device_init();
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    // nrf_log_module_filter_set(backend_id, NRF_LOG_INST_ID(test.p_log), NRF_LOG_SEVERITY_WARNING);
    // NRF_LOG_INST_DEBUG(test.p_log, "DEBUG");
    // NRF_LOG_INST_INFO(test.p_log, "INFO");
    // NRF_LOG_INST_WARNING(test.p_log, "WARNING");
    // NRF_LOG_INST_ERROR(test.p_log, "ERROR");
    // nrf_log_module_filter_set(backend_id, NRF_LOG_MODULE_ID, NRF_LOG_SEVERITY_ERROR);
    // NRF_LOG_DEBUG("Test Debug!!!!!!!!!");
    // NRF_LOG_INFO("Test Info!!!!!!!!!");
    // NRF_LOG_WARNING("Test Warning!!!!!!!!!");
    // NRF_LOG_ERROR("Test Error!!!!!!!!!");
}

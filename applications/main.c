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
#include <board.h>
#include <stdio.h>
#include "app.h"
#include "app_error.h"
#include "bsp.h"

#include "nrf_pwr_mgmt.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrfx_rtc.h"
#include "ble_app_log.h"

#include "SEGGER_RTT.h"
static int log_init(void);
static void pwr_mgmt_handle(void);

#define DK_BOARD_LED_1 LED1

int main(void)
{
    log_init();
    APP_ERROR_CHECK(nrf_pwr_mgmt_init());
    // mission_init();
    // rt_thread_idle_sethook(pwr_mgmt_handle);
    while (1) {
        // SEGGER_RTT_printf(0, "Hello s-Thread!\r\n");
        // printf("%d\n", nrfx_rtc_counter_get(&rtc_instance));
        nrf_gpio_pin_write(LED1, 0);
        rt_thread_mdelay(1000);
        nrf_gpio_pin_write(LED1, 1);
        rt_thread_mdelay(1000);
    }
    return RT_EOK;
}

static void pwr_mgmt_handle(void)
{
    const nrfx_rtc_t rtc_instance = NRFX_RTC_INSTANCE(1);
    if (NRF_LOG_PROCESS() == false) {
        // nrfx_rtc_disable(&rtc_instance);
        nrfx_rtc_tick_disable(&rtc_instance);
        nrfx_rtc_cc_set(&rtc_instance, 0, 5000, true);
        nrfx_rtc_enable(&rtc_instance);
        nrf_pwr_mgmt_run();
    }
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

static void irq_status(void)
{
    printf("irq int is: ");
    for (size_t i = 0; i < 38; i++) {
        uint32_t irq = NVIC_GetEnableIRQ(i);
        if (irq) {
            printf("%d ", i);
        }
    }
    printf("\n");
}
MSH_CMD_EXPORT(irq_status, irq status);

static void irq_enable(void)
{
    nrfx_rtc_tick_enable(&rtc_instance, true);
    nrfx_rtc_enable(&rtc_instance);

    NVIC_EnableIRQ(UARTE0_UART0_IRQn);
}
MSH_CMD_EXPORT(irq_enable, clock_int_enable);
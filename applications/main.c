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

#include "SEGGER_RTT.h"
static int log_init(void);
static void pwr_mgmt_handle(void);

int main(void)
{
    gpio_init();
    log_init();
    bsp_init();
    APP_ERROR_CHECK(nrf_pwr_mgmt_init());
    // rt_pm_release_all(PM_SLEEP_MODE_NONE);
    // rt_pm_sleep_request(PM_BOARD_ID, PM_SLEEP_MODE_LIGHT);
    max30102_init();
    ccm3310_init();
    mission_init();
    while (1) {
        nrf_gpio_pin_toggle(LED1);
        rt_thread_mdelay(2000);
    }
    return RT_EOK;
}

static int log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    // nrf_log_backend_rt_device_init();
    NRF_LOG_DEFAULT_BACKENDS_INIT();
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

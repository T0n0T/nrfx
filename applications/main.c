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

#include <nrfx_systick.h>
#include <nrf_drv_clock.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define DK_BOARD_LED_1 LED1

static int log_init(void);

int main(void)
{
    log_init();
    rt_pin_mode(DK_BOARD_LED_1, PIN_MODE_OUTPUT);
    mission_init();
    while (1) {
        NRF_LOG_INTERNAL_FLUSH();
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

    NRF_LOG_INFO("Log use RT-DEVICE as output terminal");
}

static void pwr_test(void)
{
    rt_pin_mode(3, PIN_MODE_OUTPUT);
    rt_pin_write(3, PIN_HIGH);
    rt_thread_mdelay(700);
    rt_pin_write(3, PIN_LOW);

    rt_pin_mode(22, PIN_MODE_OUTPUT);
    rt_pin_write(22, PIN_HIGH);
    rt_thread_mdelay(700);
    rt_pin_write(22, PIN_LOW);
}
MSH_CMD_EXPORT(pwr_test, test);
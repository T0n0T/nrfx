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
    while (1) {
        NRF_LOG_INTERNAL_FLUSH();
        rt_pin_write(DK_BOARD_LED_1, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(DK_BOARD_LED_1, PIN_LOW);
        rt_thread_mdelay(500);
    }
    return RT_EOK;
}

static void systic_test(void)
{
    // nrfx_systick_init();
    // nrf_systick_csr_set(NRF_SYSTICK_CSR_CLKSOURCE_REF | NRF_SYSTICK_CSR_TICKINT_ENABLE | NRF_SYSTICK_CSR_ENABLE);
    printf("systick csr : %ld\n", nrf_systick_csr_get());
    printf("systick val: %ld\n", nrf_systick_val_get());
    printf("systick load: %ld\n", nrf_systick_load_get());
    printf("hfclk is: %d\n", nrfx_clock_hfclk_is_running());
    printf("lfclk is: %d\n", nrfx_clock_lfclk_is_running());
    for (size_t i = 0; i < 10; i++) {
        nrfx_systick_delay_ms(500);
        printf("systick 500ms!\n");
    }
}
MSH_CMD_EXPORT(systic_test, test);

static int log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    // nrf_log_backend_rt_device_init();
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("Log use RT-DEVICE as output terminal");
}
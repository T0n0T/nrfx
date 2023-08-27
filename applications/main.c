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

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_backend_rtt.h"
#define DK_BOARD_LED_1 17

static int hwtimer_mission(void);

int main(void)
{
    rt_pin_mode(DK_BOARD_LED_1, PIN_MODE_OUTPUT);
    while (1) {
        rt_pin_write(DK_BOARD_LED_1, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(DK_BOARD_LED_1, PIN_LOW);
        rt_thread_mdelay(500);
    }
    return RT_EOK;
}

#include <drv_spim.h>

static int w5500_bing_spi(void)
{
    return rt_hw_spi_device_attach("spi2", WIZ_SPI_DEVICE, BSP_SPI2_SS_PIN);
}
INIT_DEVICE_EXPORT(w5500_bing_spi);

static rt_err_t hwtimer_call(rt_device_t dev, rt_size_t size)
{
}

static int hwtimer_mission(void)
{
    rt_hwtimer_mode_t mode = HWTIMER_MODE_PERIOD;
    rt_device_t timer      = rt_device_find("timer1");
    if (timer == RT_NULL) {
        /* todo */
        return -1;
    }
    if (rt_device_set_rx_indicate(timer, hwtimer_call)) {
        printf("%s set callback failed!", timer->parent.name);
        return -1;
    }

    if (rt_device_control(device->timer, HWTIMER_CTRL_MODE_SET, &mode) != RT_EOK) {
        printf("%s set mode failed!", timer->parent.name);
        return -1;
    }
    if (rt_device_open(timer, RT_DEVICE_OFLAG_RDWR) != RT_EOK) {
        printf("open %s device failed!", timer->parent.name);
        return -1;
    }
}

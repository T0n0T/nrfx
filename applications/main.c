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
#define DK_BOARD_LED_1 17

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
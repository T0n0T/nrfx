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

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define DK_BOARD_LED_1 LED1
char *test[] = {
    "umount - Unmount the mountpoint",
    "df - disk free",
    "echo - echo string to file",
    "tail - print the last N - lines data of the given file",
    "at - RT-Thread AT component cli: at <server|client [dev_name]>",
    "ifconfig - list the information of all network interfaces",
    "ping - ping network host",
    "dns - list and set the information of dns",
    "netstat - list the information of TCP / IP",
    "reboot - Reboot System",
};

static int log_init(void);
void ble_log_flush_process(void);

int main(void)
{
    log_init();
    APP_ERROR_CHECK(nrf_pwr_mgmt_init());
    rt_pin_mode(DK_BOARD_LED_1, PIN_MODE_OUTPUT);
    // mission_init();
    while (1) {
        NRF_LOG_INTERNAL_FLUSH();
        ble_log_flush_process();
        nrf_pwr_mgmt_run();
        rt_pin_write(DK_BOARD_LED_1, PIN_HIGH);
        rt_thread_mdelay(50);
        rt_pin_write(DK_BOARD_LED_1, PIN_LOW);
        rt_thread_mdelay(50);
    }
    return RT_EOK;
}

uint8_t isinit = 0;
static struct rt_ringbuffer log_ringbuffer;
static char log_buf[128 * 2];
uint32_t max_data_len = 50;

// #include "SEGGER_RTT.h"

static int log_init(void)
{
    rt_ringbuffer_init(&log_ringbuffer, log_buf, sizeof(log_buf));
    isinit = 1;
    // ret_code_t err_code = NRF_LOG_INIT(NULL);
    // APP_ERROR_CHECK(err_code);
    // // nrf_log_backend_rt_device_init();
    // NRF_LOG_DEFAULT_BACKENDS_INIT();

    // NRF_LOG_INFO("Log use RT-DEVICE as output terminal");
}

void rt_hw_console_output(const char *str)
{
    // SEGGER_RTT_printf(0, "%s", str);
    // rt_device_write(jlink_dev, 0, str, rt_strlen(str));
    if (isinit) {
        rt_ringbuffer_put_force(&log_ringbuffer, str, rt_strlen(str)); // 循环更新日志buff
    }
}

void ble_log_flush_process(void)
{
    uint32_t err_code;
    static uint8_t data_array[256];
    uint16_t data_len = 0;
    uint8_t *index    = 0;
    data_len          = rt_ringbuffer_get(&log_ringbuffer, data_array, 256); // 获取日志数据
    index             = &data_array[0];

    // 接收日志数据，当接收的数据长度达到max_data_len或者接收到换行符后认为一包数据接收完成
    while (data_len > 0) {
        if (data_len >= max_data_len) {
            NRF_LOG_DEBUG("Ready to send data len %d over BLE LOG", max_data_len);
            // 日志接收的数据使用notify发送给BLE主机
            rt_device_write(jlink_dev, 0, index, max_data_len);
            index += max_data_len;
            data_len -= max_data_len;
        } else {
            NRF_LOG_DEBUG("Ready to send data len %d over BLE LOG", data_len);
            // 日志接收的数据使用notify发送给BLE主机
            rt_device_write(jlink_dev, 0, index, data_len);
            break;
        }
    }
}

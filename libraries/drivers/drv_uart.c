/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-28     xckhmf       Modify for <nrfx>
 * 2021-06-26     supperthomas fix rt_hw_uart_init
 *
 */
#include <rtdevice.h>
#include "drv_uart.h"

#ifdef BSP_USING_UART0
nrfx_uart_t uart0               = NRFX_UART_INSTANCE(0);
nrfx_uart_config_t uart0_config = NRFX_UART_DEFAULT_CONFIG;

static struct rt_serial_device _serial_0;
static uint8_t rx_ch;
static struct rt_ringbuffer uart0_ringbuffer;
static uint8_t uart0_buf[RT_SERIAL_RB_BUFSZ];

#define UART_RETRY_TIME (1000000)
#define UART_WAIT(expression, retry, result) \
    result = 0;                              \
    while (expression) {                     \
        retry--;                             \
        rt_hw_us_delay(50);                  \
        if (retry == 0) {                    \
            result = 1;                      \
            break;                           \
        }                                    \
    }

void uart0_event_hander(nrfx_uart_event_t const *p_event, void *p_context)
{
    if (p_event->type == NRFX_UART_EVT_RX_DONE) {
        nrfx_uart_rx(&uart0, &rx_ch, 1);
        rt_ringbuffer_put_force(&uart0_ringbuffer, &rx_ch, 1);
        if (p_event->data.rxtx.bytes == 1) {
            rt_hw_serial_isr(&_serial_0, RT_SERIAL_EVENT_RX_IND);
        }
    }
    if (p_event->type == NRFX_UART_EVT_TX_DONE) {
        /* @TODO:[RT_DEVICE_FLAG_INT_TX]*/
    }
}

static rt_err_t _uart_cfg(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);

    nrfx_uart_uninit(&uart0);

    switch (cfg->baud_rate) {
        case 115200:
            uart0_config.baudrate = NRF_UART_BAUDRATE_115200;
            break;

        case 9600:
            uart0_config.baudrate = NRF_UART_BAUDRATE_9600;
            break;

        default:
            uart0_config.baudrate = NRF_UART_BAUDRATE_115200;
            break;
    }

    if (cfg->parity == PARITY_NONE) {
        uart0_config.parity = NRF_UART_PARITY_EXCLUDED;
    } else {
        uart0_config.parity = NRF_UART_PARITY_INCLUDED;
    }

    uart0_config.hwfc    = NRF_UART_HWFC_DISABLED;
    uart0_config.pseltxd = BSP_UART0_TX_PIN;
    uart0_config.pselrxd = BSP_UART0_RX_PIN;

    nrfx_uart_init(&uart0, &uart0_config, uart0_event_hander);
    nrfx_uart_rx(&uart0, &rx_ch, 1);
    nrf_uart_int_disable(uart0.p_reg, NRF_UART_INT_MASK_TXDRDY);
    return RT_EOK;
}

static rt_err_t _uart_ctrl(struct rt_serial_device *serial, int cmd, void *arg)
{
    return RT_EOK;
}

static int _uart_putc(struct rt_serial_device *serial, char c)
{
    int rtn = 1;
    int retry = UART_RETRY_TIME;
    RT_ASSERT(serial != RT_NULL);

    nrf_uart_event_clear(uart0.p_reg, NRF_UART_EVENT_TXDRDY);
    nrf_uart_task_trigger(uart0.p_reg, NRF_UART_TASK_STARTTX);
    nrf_uart_txd_set(uart0.p_reg, (uint8_t)c);
    UART_WAIT(!nrf_uart_event_check(uart0.p_reg, NRF_UART_EVENT_TXDRDY), retry, rtn);
    return rtn;
}

static int _uart_getc(struct rt_serial_device *serial)
{
    char ch = -1;
    RT_ASSERT(serial != RT_NULL);
    if (rt_ringbuffer_getchar(&uart0_ringbuffer, &ch)) {
        return (int)ch;
    } else {
        return -1;
    }
}

static struct rt_uart_ops _uart_ops = {
    _uart_cfg,
    _uart_ctrl,
    _uart_putc,
    _uart_getc};

int rt_hw_uart_init(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_ringbuffer_init(&uart0_ringbuffer, uart0_buf, sizeof(uart0_buf));
    _serial_0.config = config;
    _serial_0.ops    = &_uart_ops;
    nrfx_uart_init(&uart0, &uart0_config, uart0_event_hander);
    return rt_hw_serial_register(&_serial_0, "uart0",
                                 RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX, RT_NULL);
}

#endif /* BSP_USING_UART0 */
#ifndef _UART_H_
#define _UART_H_

#include <nrfx_uart.h>

extern nrfx_uart_t uart0;
extern nrfx_uart_config_t uart0_config;
extern void uart0_event_hander(nrfx_uart_event_t const *p_event, void *p_context);

#define RT_DEVICE_CTRL_CUSTOM        0x20
#define RT_DEVICE_CTRL_PIN           0x21
#define RT_DEVICE_POWERSAVE          0x22
#define RT_DEVICE_WAKEUP             0x23

#define UART_CONFIG_BAUD_RATE_9600   1
#define UART_CONFIG_BAUD_RATE_115200 2

#define UART0_RB_SIZE                1024

int rt_hw_uart_init(void);

#endif

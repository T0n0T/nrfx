/**
 * @file btn_event.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-12
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <rtdevice.h>
#include <board.h>
#include <stdio.h>
#include <app.h>

#include <button.h>
#include "nrfx_pwm.h"
#include "nrfx_gpiote.h"
#include <nrfx_systick.h>
#include "drv_uart.h"
#include "nrf_pwr_mgmt.h"

#define DBG_LVL DBG_LOG
#define DBG_TAG "btn"
#include <rtdbg.h>

rt_device_t dev;
/* button */
Button_t SW_BUTTON;
static struct rt_thread btn_thread;
static char btn_stack[1024];

rt_uint8_t read_sw_btn(void)
{
    return nrf_gpio_pin_read(SW);
}

static void sw_irq_handler(void *args)
{
    nrfx_rtc_disable(&rtc_instance);
    nrfx_rtc_cc_disable(&rtc_instance, 0);
    nrfx_rtc_tick_enable(&rtc_instance, true);
    nrfx_rtc_enable(&rtc_instance);

    // rt_device_control(dev, RT_DEVICE_WAKEUP, RT_NULL);
    NVIC_EnableIRQ(UARTE0_UART0_IRQn);
    rt_pin_write(LED2, PIN_HIGH);
    // LOG_D("exit pwr!");
}

void btn_click(void)
{
    LOG_D("single click!");
    // beep_on();
    // rt_thread_mdelay(50);
    // beep_off();
    // if (mission_status) {
    //     publish_handle();
    // }
}

void btn_double(void)
{
    static int flag = 1;
    LOG_D("double click!");
    beep_on();
    rt_thread_mdelay(25);
    beep_off();
    rt_thread_mdelay(50);
    beep_on();
    rt_thread_mdelay(25);
    beep_off();

    // if (sm4_flag == 0) {
    //     printf("sm4 encrypt mode ON !!!!!!!!!\n");
    //     sm4_flag = 1;
    // } else {
    //     printf("sm4 encrypt mode OFF!!!!!!!!!\n");
    //     sm4_flag = 0;
    // }
}

void btn_long_free(void)
{
    LOG_D("long click!");
    beep_on();

    nrf_systick_csr_set(NRF_SYSTICK_CSR_CLKSOURCE_REF | NRF_SYSTICK_CSR_TICKINT_DISABLE | NRF_SYSTICK_CSR_DISABLE);
    nrfx_uart_uninit(&uart0);
    nrfx_gpiote_init();
    nrfx_gpiote_in_config_t cfg = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(0);
    nrfx_gpiote_in_init(14, &cfg, sw_irq_handler);
    nrfx_gpiote_in_event_enable(14, true);
    rt_thread_mdelay(500);
    // rt_hw_cpu_reset();

    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_STAY_IN_SYSOFF);
}

static void btn_entry(void *p)
{
    beep_init();
    nrf_gpio_cfg_input(SW, NRF_GPIO_PIN_PULLUP);
    Button_Create(
        "SW",
        &SW_BUTTON,
        read_sw_btn,
        PIN_LOW);
    Button_Attach(&SW_BUTTON, BUTTON_DOWM, btn_click);
    Button_Attach(&SW_BUTTON, BUTTON_DOUBLE, btn_double);
    Button_Attach(&SW_BUTTON, BUTTON_LONG_FREE, btn_long_free);

    uint32_t s;
    while (1) {
        Button_Cycle_Process(&SW_BUTTON);
        rt_thread_mdelay(30);
    }
}

int btn_init(void)
{
    if (rt_thread_init(&btn_thread, "btn", btn_entry, 0, btn_stack, sizeof(btn_stack), 10, 5) != RT_EOK) {
        LOG_E("btn thread init fail!\n");
        return -1;
    }
    return rt_thread_startup(&btn_thread);
}
INIT_APP_EXPORT(btn_init);

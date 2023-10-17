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
#include "bsp.h"
#include "drv_uarte.h"
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

void btn_click(void)
{
    LOG_D("single click!");
    // beep_on();
    // rt_thread_mdelay(50);
    // beep_off();
    // if (mission_status) {
    //     publish_handle();
    // }
    LOG_D("enter pwr!");
    // nrfx_rtc_disable(&rtc_instance);
    nrfx_rtc_tick_disable(&rtc_instance);
    // nrfx_rtc_cc_set(&rtc_instance, 0, 5000, true);
    // nrfx_rtc_enable(&rtc_instance);
    dev = rt_console_get_device();
    rt_device_control(dev, RT_DEVICE_POWERSAVE, RT_NULL);
    bsp_uninit();
    set_sleep_exit_pin();
    NVIC_DisableIRQ(UARTE0_UART0_IRQn);
    nrf_pwr_mgmt_run();
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

    if (sm4_flag == 0) {
        printf("sm4 encrypt mode ON !!!!!!!!!\n");
        sm4_flag = 1;
    } else {
        printf("sm4 encrypt mode OFF!!!!!!!!!\n");
        sm4_flag = 0;
    }
}

void btn_long_free(void)
{
    LOG_D("long click!");
    beep_on();
    dev = rt_console_get_device();
    rt_device_control(dev, RT_DEVICE_POWERSAVE, RT_NULL);
    bsp_uninit();
    // nrf_gpio_cfg_input(14, NRF_GPIO_PIN_PULLUP); // 将要唤醒的脚配置为输入
    // /* 设置为上升沿检出，这个一定不要配置错了 */
    // nrf_gpio_pin_sense_t sense = NRF_GPIO_PIN_SENSE_HIGH;
    // nrf_gpio_cfg_sense_set(14, sense);
    rt_thread_mdelay(500);
    // rt_hw_cpu_reset();

    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_STAY_IN_SYSOFF);
}

static void btn_entry(void *p)
{
    nrf_gpio_pin_write(LED2, 1);
    nrf_gpio_pin_write(LED3, 1);
    bsp_init();
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

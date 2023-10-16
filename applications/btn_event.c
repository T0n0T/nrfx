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
#include "nrf_gpio.h"
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

/* pwm beep */
static nrfx_pwm_t m_pwm0                     = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_common_t seq0_values[] = {125};

static void beep_init(void)
{
    // 定义PWM初始化配置结构体并初始化参数
    nrfx_pwm_config_t const config0 =
        {
            .output_pins =
                {
                    BEEP,                  // channel 0 -> pin beep
                    NRFX_PWM_PIN_NOT_USED, // channel 1
                    NRFX_PWM_PIN_NOT_USED, // channel 2
                    NRFX_PWM_PIN_NOT_USED  // channel 3
                },
            .irq_priority = APP_IRQ_PRIORITY_LOWEST,
            .base_clock   = NRF_PWM_CLK_1MHz, // 1mhz -> 1us; 4khz -> 250us = 250 * 1us
            .count_mode   = NRF_PWM_MODE_UP,
            .top_value    = 250,                 // max period num
            .load_mode    = NRF_PWM_LOAD_COMMON, // common load seq
            .step_mode    = NRF_PWM_STEP_AUTO,
        };

    if (nrfx_pwm_init(&m_pwm0, &config0, NULL) != NRFX_SUCCESS) {
        printf("pwm init failed\n");
    }
}

static void beep_on(void)
{
    static nrf_pwm_sequence_t const seq0 =
        {
            .values.p_common = seq0_values,
            .length          = NRF_PWM_VALUES_LENGTH(seq0_values),
            .repeats         = 0,
            .end_delay       = 0,
        };
    (void)nrfx_pwm_simple_playback(&m_pwm0, &seq0, 2000, NRFX_PWM_FLAG_STOP);
}

static void beep_off(void)
{
    nrfx_pwm_stop(&m_pwm0, 1);
}

rt_uint8_t read_sw_btn(void)
{
    // return nrf_gpio_pin_read(SW);
    return rt_pin_read(SW);
}

static void sw_irq_handler(void *args)
{
    nrfx_rtc_tick_enable(&rtc_instance, true);
    nrfx_rtc_enable(&rtc_instance);
    rt_device_control(dev, RT_DEVICE_WAKEUP, RT_NULL);
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
    // LOG_D("enter pwr!");
    // nrfx_rtc_disable(&rtc_instance);
    // nrfx_rtc_tick_disable(&rtc_instance);
    // dev = rt_console_get_device();
    // rt_device_control(dev, RT_DEVICE_POWERSAVE, RT_NULL);
    // nrf_pwr_mgmt_run();
    // rt_pin_write(LED2, PIN_LOW);
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
    rt_thread_mdelay(500);
    // rt_hw_cpu_reset();
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
}

static void btn_entry(void *p)
{
    rt_pin_mode(POWER_KEEP, PIN_MODE_OUTPUT);
    rt_pin_mode(LED2, PIN_MODE_OUTPUT);
    rt_pin_mode(LED3, PIN_MODE_OUTPUT);
    rt_pin_mode(SW, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(14, PIN_IRQ_MODE_FALLING, sw_irq_handler, RT_NULL);
    rt_pin_irq_enable(14, PIN_IRQ_ENABLE);
    // nrf_gpio_cfg_input(SW, NRF_GPIO_PIN_PULLUP);

    rt_pin_write(LED2, PIN_HIGH);
    rt_pin_write(LED3, PIN_HIGH);
    beep_init();
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

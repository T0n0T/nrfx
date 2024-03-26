/**
 * @file bsp.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-10-24
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <bsp.h>
#include <stdio.h>

#include "ccm3310.h"
#include "max30102.h"
#include "ec800m.h"
#include "button.h"
#include <nrfx_power.h>
#include <nrfx_systick.h>
#include "nrf_drv_saadc.h"

#include "FreeRTOS.h"
#include "timers.h"

ec800m_t*            ec800m;
static TimerHandle_t leds_tmr;
extern uint8_t       working_flag;
extern uint8_t       battery_flag;

/** gpio */
void gpio_init(void)
{
    nrf_gpio_cfg_output(LED1);
    nrf_gpio_cfg_output(LED2);
    nrf_gpio_cfg_output(LED3);

    nrf_gpio_cfg_output(EC800_PIN_PWREN);
    nrf_gpio_pin_clear(EC800_PIN_PWREN);

    nrf_gpio_pin_write(LED1, 1);
    nrf_gpio_pin_write(LED2, 1);
    nrf_gpio_pin_write(LED3, 1);

    nrf_gpio_cfg_output(POR);
    nrf_gpio_cfg_output(GINT0);
    nrf_gpio_pin_write(GINT0, 1);
    nrf_gpio_cfg_input(GINT1, NRF_GPIO_PIN_NOPULL);

    nrfx_gpiote_init();
}

void set_sleep_exit_pin(void)
{
    nrf_gpio_cfg_input(SW, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_sense_t sense = NRF_GPIO_PIN_SENSE_LOW;
    nrf_gpio_cfg_sense_set(SW, sense);
}

void gpio_uninit(void)
{
    nrf_gpio_cfg_default(LED1);
    nrf_gpio_cfg_default(LED2);
    nrf_gpio_cfg_default(LED3);

    nrf_gpio_cfg_default(POWER_KEEP);
    nrf_gpio_cfg_default(SW);

    nrf_gpio_cfg_default(POR);
    nrf_gpio_cfg_default(GINT0);
    nrf_gpio_cfg_default(GINT1);
}

/** pwm beep */
static nrfx_pwm_t              m_pwm0        = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_common_t seq0_values[] = {125};

void beep_init(void)
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

void beep_uninit(void)
{
    nrfx_pwm_uninit(&m_pwm0);
}

void beep_on(void)
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

void beep_off(void)
{
    nrfx_pwm_stop(&m_pwm0, 1);
}

/** adc */
void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
   
}

void adc_init(void)
{
    ret_code_t                 err_code;
    nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(ADC_BATTERY);
    err_code                                  = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);
    err_code = nrfx_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);
}

void adc_read(void)
{
    int16_t saadc_val;
    nrfx_saadc_sample_convert(0, &saadc_val);
}

void bsp_uninit(void)
{
    nrf_gpio_pin_clear(EC800_PIN_PWREN);
    gpio_uninit();
    beep_uninit();
    twim_uninit();
    spim_uninit();
    extern nrfx_uart_t p_instance;
    nrfx_uart_uninit(&p_instance);
}

/**
 * @brief Handle events from leds timer.
 *
 * @note Timer handler does not support returning an error code.
 * Errors from bsp_led_indication() are not propagated.
 *
 * @param[in]   p_context   parameter registered in timer start function.
 */
static void leds_timer_handler(TimerHandle_t xTimer)
{
    static flag = 0;
    if (!flag) {
        nrf_gpio_pin_toggle(LED_RUN);
        if (battery_flag) {
            nrf_gpio_pin_toggle(LED_BATTERY_LOW);
        }
        flag = 1;
    } else {
        flag = 0;
    }

    if (working_flag) {
        nrf_gpio_pin_toggle(LED_DATA);
    }
}

void bsp_init(void)
{
    ret_code_t err_code = NRF_SUCCESS;
    gpio_init();

    beep_init();
    // // leds_tmr = xTimerCreate("leds", pdMS_TO_TICKS(1000), pdTRUE, NULL, leds_timer_handler);
    // // // xTimerStart(leds_tmr, 0);
    btn_init();
    ec800m = ec800m_init(); // 440 uA
    ccm3310_init();         // 260 ua, 4 pins low

    max30102_init();
    nrf_uart_disable(NRF_UART0);
}

void PRE_SLEEP(void)
{
    // nrf_uart_disable(NRF_UART0);
}

void POST_SLEEP(void)
{
    // nrf_uart_enable(NRF_UART0);
}
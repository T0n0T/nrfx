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
#include "button.h"
#include <nrfx_power.h>
#include <nrfx_systick.h>

#include "FreeRTOS.h"
#include "app_timer.h"

/** gpio */
void gpio_init(void)
{
    nrf_gpio_cfg_output(LED1);
    nrf_gpio_cfg_output(LED2);
    nrf_gpio_cfg_output(LED3);

    nrf_gpio_cfg_output(POWER_KEEP);

    nrf_gpio_pin_write(LED2, 1);
    nrf_gpio_pin_write(LED3, 1);

    nrf_gpio_cfg_output(POR);
    nrf_gpio_cfg_output(GINT0);
    nrf_gpio_cfg_input(GINT1, NRF_GPIO_PIN_NOPULL);

    // nrf_gpio_pin_write(POR, 0);
    // NRFX_DELAY_US(50000);
    // nrf_gpio_pin_write(POR, 1);
}

void set_sleep_exit_pin(void)
{
    nrf_gpio_cfg_input(13, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_sense_t sense = NRF_GPIO_PIN_SENSE_LOW;
    nrf_gpio_cfg_sense_set(13, sense);
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
static nrfx_pwm_t m_pwm0                     = NRFX_PWM_INSTANCE(0);
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

void bsp_sleep(void)
{
    // nrfx_clock_hfclk_start();
    // gpio_init();
    // beep_init();
    // ccm3310_init();
    // i2c0_init();

    // nrfx_uart_init(&uart0, &uart0_config, uart0_event_hander);
}

void bsp_uninit(void)
{
    // gpio_uninit();
    beep_uninit();
    // ccm3310_uninit();
    // i2c0_uninit();
    // nrfx_uart_uninit(&uart0);
    // nrfx_clock_hfclk_stop();
}

APP_TIMER_DEF(leds_tmr);

/**
 * @brief Handle events from leds timer.
 *
 * @note Timer handler does not support returning an error code.
 * Errors from bsp_led_indication() are not propagated.
 *
 * @param[in]   p_context   parameter registered in timer start function.
 */
static void leds_timer_handler(void *p_context)
{
    nrf_gpio_pin_toggle(LED1);
}

void bsp_init(void)
{
    ret_code_t err_code = NRF_SUCCESS;
    gpio_init();

    // beep_init();
    APP_ERROR_CHECK(app_timer_create(&leds_tmr, APP_TIMER_MODE_REPEATED, leds_timer_handler));
    APP_ERROR_CHECK(app_timer_start(leds_tmr, APP_TIMER_TICKS(1000), NULL));

    btn_init();
}
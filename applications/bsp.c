/**
 * @file bsp.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-17
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <bsp.h>
#include <board.h>
#include <stdio.h>

#include "ccm3310.h"
#include "drv_uart.h"
#include "drv_i2c.h"
#include <nrfx_power.h>
#include <nrfx_systick.h>

/** gpio */
static void sw_handle(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
void gpio_init(void)
{
    nrf_gpio_cfg_output(LED1);
    nrf_gpio_cfg_output(LED2);
    nrf_gpio_cfg_output(LED3);

    nrf_gpio_cfg_output(POWER_KEEP);
    nrf_gpio_cfg_input(SW, NRF_GPIO_PIN_PULLUP);

    nrf_gpio_pin_write(LED2, 1);
    nrf_gpio_pin_write(LED3, 1);

    nrf_gpio_cfg_output(POR);
    nrf_gpio_cfg_output(GINT0);
    nrf_gpio_cfg_input(GINT1, NRF_GPIO_PIN_NOPULL);

    nrf_gpio_cfg_output(EC800_PWR_EN);
    nrf_gpio_pin_write(EC800_PWR_EN, 0);
}

void set_sleep_exit_pin(void)
{
    nrf_gpio_cfg_input(13, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_sense_t sense = NRF_GPIO_PIN_SENSE_LOW;
    nrf_gpio_cfg_sense_set(13, sense);
}

void show_pin(void)
{
    printf("SW sense is: %d\n", nrf_gpio_pin_sense_get(SW));
    printf("SW dir   is: %d\n", nrf_gpio_pin_dir_get(SW));
    printf("SW pull  is: %d\n", nrf_gpio_pin_pull_get(SW));
    printf("SW input is: %d\n", nrf_gpio_pin_input_get(SW));
    printf("SW level is: %d\n", nrf_gpio_pin_read(SW));
}
MSH_CMD_EXPORT(show_pin, show pin);

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

/** rtc1 -> tick */
const nrfx_rtc_t rtc = NRFX_RTC_INSTANCE(1);
int flag             = 0;
void rtc1_schedule_handle(nrfx_rtc_int_type_t int_type)
{
    rt_interrupt_enter();
    switch (int_type) {
        case NRFX_RTC_INT_TICK:
            rt_tick_increase();
            break;
        case NRFX_RTC_INT_COMPARE0:
            flag = 1;
            break;
        default:
            break;
    }
    rt_interrupt_leave();
}

void rtc_tick_configure(void)
{
    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);
    nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
    config.prescaler         = RTC_PRESCALER;
    nrfx_rtc_init(&rtc, &config, rtc1_schedule_handle);
    nrfx_rtc_tick_enable(&rtc, true);
    nrfx_rtc_enable(&rtc);
}

void bsp_init(void)
{
    // nrfx_clock_hfclk_start();
    // gpio_init();
    beep_init();
    // ccm3310_init();
    i2c0_init();

    // nrfx_uart_init(&uart0, &uart0_config, uart0_event_hander);
}

void bsp_uninit(void)
{
    // gpio_uninit();
    beep_uninit();
    // ccm3310_uninit();
    i2c0_uninit();
    // nrfx_uart_uninit(&uart0);
    // nrfx_clock_hfclk_stop();
}

void bsp_sleep(int argc, char **argv)
{
    uint32_t timeout = 0;
    if (argc >= 2) {
        timeout = atoi(argv[1]);
    }

    rt_interrupt_enter();
    flag = 0;
    bsp_uninit();

    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);
    nrf_rtc_int_disable(NRF_RTC1, NRF_RTC_INT_TICK_MASK);

    /* Configure CTC interrupt */
    nrf_rtc_cc_set(NRF_RTC1, 0, timeout);
    // nrfx_rtc_counter_clear(&rtc);
    nrf_rtc_task_trigger(NRF_RTC1, NRF_RTC_TASK_CLEAR);
    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);
    nrf_rtc_int_enable(NRF_RTC1, NRF_RTC_INT_COMPARE0_MASK);

    __DSB();
    do {
        __WFE();
    } while (0 == flag);

    nrf_rtc_int_disable(NRF_RTC1, NRF_RTC_INT_COMPARE0_MASK);
    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);
    bsp_init();
    printf("counter: %d\n", nrf_rtc_counter_get(NRF_RTC1));
    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_TICK);
    nrf_rtc_int_enable(NRF_RTC1, NRF_RTC_INT_TICK_MASK);

    NVIC_ClearPendingIRQ(RTC1_IRQn);
    rt_interrupt_leave();
}
MSH_CMD_EXPORT(bsp_sleep, bsp sleep);
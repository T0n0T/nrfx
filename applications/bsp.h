/**
 * @file bsp.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-17
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "nrfx_pwm.h"
#include "nrfx_gpiote.h"
#include "nrf_drv_clock.h"
#include "nrfx_rtc.h"

extern int flag;
extern const nrfx_rtc_t rtc_instance;

#define LF_CLK_HZ     (32768UL)
#define RTC_PRESCALER ((uint32_t)(NRFX_ROUNDED_DIV(LF_CLK_HZ, RT_TICK_PER_SECOND) - 1))

void gpio_init(void);
void gpio_uninit(void);
void set_sleep_exit_pin(void);
void release_sleep_exit_pin(void);
void beep_init(void);
void beep_uninit(void);
void beep_on(void);
void beep_off(void);

void rtc_tick_configure(void);

void bsp_init(void);
void bsp_uninit(void);
void bsp_sleep(void);

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
#ifndef __BSP_H__
#define __BSP_H__

#include "nrfx_pwm.h"
#include "nrfx_gpiote.h"
#include "nrf_drv_clock.h"
#include "nrfx_rtc.h"

#define LED_ON(x)                      \
    do {                               \
        nrf_gpio_pin_clear((uint32_t)x); \
    } while (0);

#define LED_OFF(x)                       \
    do {                                 \
        nrf_gpio_pin_set((uint32_t)x); \
    } while (0);

extern int              flag;
extern const nrfx_rtc_t rtc_instance;

void gpio_init(void);
void gpio_uninit(void);
void set_sleep_exit_pin(void);
void release_sleep_exit_pin(void);
void beep_init(void);
void beep_uninit(void);
void beep_on(void);
void beep_off(void);

void btn_init(void);

void bsp_init(void);
void bsp_uninit(void);

#endif

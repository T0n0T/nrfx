/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-29     supperthomas first version
 *
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>

#include "bsp.h"
#include "board.h"
#include "drv_uart.h"
#include "nrf_gpio.h"
#include <nrfx_systick.h>

/*
 * This is the timer interrupt service routine.
 *
 */
void SysTick_Handler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
    // rt_kprintf("systick event!\n");
}

const nrfx_rtc_t rtc_instance = NRFX_RTC_INSTANCE(1);

void rtc1_schedule_handle(nrfx_rtc_int_type_t int_type)
{
    rt_interrupt_enter();
    switch (int_type) {
        case NRFX_RTC_INT_TICK:

            rt_tick_increase();
            break;
        case NRFX_RTC_INT_COMPARE0:
            nrf_gpio_pin_toggle(LED1);
        default:
            break;
    }
    rt_interrupt_leave();
}

void RTC_tick_configure(void)
{
    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);
    nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
    config.prescaler         = 4095;
    nrfx_rtc_init(&rtc_instance, &config, rtc1_schedule_handle);
    nrfx_rtc_cc_set(&rtc_instance, 0, 16, true);
    nrfx_rtc_enable(&rtc_instance);
}

void SysTick_Configuration(void)
{
    /* Set interrupt priority */
    NVIC_SetPriority(SysTick_IRQn, 0xf);

    /* Configure SysTick to interrupt at the requested rate. */
    nrf_systick_load_set(SystemCoreClock / RT_TICK_PER_SECOND);
    nrf_systick_val_clear();
    nrf_systick_csr_set(NRF_SYSTICK_CSR_CLKSOURCE_REF | NRF_SYSTICK_CSR_TICKINT_ENABLE | NRF_SYSTICK_CSR_ENABLE);
}

/**
 * The time delay function.
 *
 * @param microseconds.
 */
void rt_hw_us_delay(uint32_t us)
{
    nrfx_systick_delay_us(us);
}

void rt_hw_board_init(void)
{
    rt_hw_interrupt_enable(0);
    // sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
    /* Activate deep sleep mode */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    SysTick_Configuration();

    rtc_tick_configure();

#if defined(RT_USING_HEAP)
    rt_system_heap_init((void *)HEAP_BEGIN, (void *)HEAP_END);
#endif

#ifdef RT_USING_SERIAL
    rt_hw_uart_init();
#endif

#if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
    rt_hw_jlink_console_init();
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif

#ifdef BSP_USING_SOFTDEVICE
    extern uint32_t Image$$RW_IRAM1$$Base;
    uint32_t const *const m_ram_start = &Image$$RW_IRAM1$$Base;
    if ((uint32_t)m_ram_start == 0x20000000) {
        rt_kprintf("\r\nusing softdevice the RAM couldn't be %p,please use the templete from package\r\n", m_ram_start);
        while (1)
            ;
    } else {
        rt_kprintf("\r\nusing softdevice the RAM at %p\r\n", m_ram_start);
    }
#endif
}

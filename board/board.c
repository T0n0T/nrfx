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

#include "board.h"
#include "drv_uart.h"
#include <nrfx_systick.h>
#include <nrf_drv_clock.h>

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
            nrfx_rtc_counter_clear(&rtc_instance);
            nrfx_rtc_disable(&rtc_instance);
            nrfx_rtc_cc_disable(&rtc_instance, 0);
            nrfx_rtc_tick_enable(&rtc_instance, true);
            nrfx_rtc_enable(&rtc_instance);
            NVIC_EnableIRQ(UARTE0_UART0_IRQn);
            rt_pin_write(LED2, PIN_HIGH);
        default:
            break;
    }
    rt_interrupt_leave();
    // rt_kprintf("clk_event!\n");
}

void RTC_tick_configure(void)
{

    NVIC_SetPriority(RTC1_IRQn, 5);

    nrf_clock_lf_src_set((nrf_clock_lfclk_t)NRFX_CLOCK_CONFIG_LF_SRC);
    nrfx_clock_lfclk_start();

    // Initialize RTC instance
    nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
    config.prescaler         = RTC_PRESCALER;

    nrfx_rtc_init(&rtc_instance, &config, rtc1_schedule_handle);

    nrfx_rtc_tick_enable(&rtc_instance, true);
    nrfx_rtc_cc_set(&rtc_instance, 0, 5000, true);
    // Power on RTC instance
    nrfx_rtc_enable(&rtc_instance);
}

void SysTick_Configuration(void)
{
    // nrf_drv_clock_init();
    // nrfx_clock_init(clk_event_handler);
    // nrfx_clock_enable();
    // // nrfx_clock_lfclk_start();
    // nrfx_clock_hfclk_start();
    // /* Set interrupt priority */
    NVIC_SetPriority(SysTick_IRQn, 0xf);

    // /* Configure SysTick to interrupt at the requested rate. */
    nrf_systick_load_set(SystemCoreClock / RT_TICK_PER_SECOND);
    nrf_systick_val_clear();
    nrf_systick_csr_set(NRF_SYSTICK_CSR_CLKSOURCE_REF | NRF_SYSTICK_CSR_TICKINT_ENABLE | NRF_SYSTICK_CSR_ENABLE);
}

/**
 * The time delay function.
 *
 * @param microseconds.
 */
void rt_hw_us_delay(rt_uint32_t us)
{
    rt_uint32_t ticks;
    rt_uint32_t told, tnow, tcnt = 0;
    rt_uint32_t reload = SysTick->LOAD;

    ticks = us * reload / (1000000 / RT_TICK_PER_SECOND);
    told  = SysTick->VAL;
    while (1) {
        tnow = SysTick->VAL;
        if (tnow != told) {
            if (tnow < told) {
                tcnt += told - tnow;
            } else {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks) {
                break;
            }
        }
    }
}

void rt_hw_board_init(void)
{
    rt_hw_interrupt_enable(0);
    sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
    /* Activate deep sleep mode */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    // SysTick_Configuration();

    RTC_tick_configure();

#if defined(RT_USING_HEAP)
    rt_system_heap_init((void *)HEAP_BEGIN, (void *)HEAP_END);
#endif

#ifdef RT_USING_SERIAL
    rt_hw_uart_init();
#endif

#if defined(RT_USING_CONSOLE) && defined(RT_USING_DEVICE)
    // rt_hw_jlink_console_init();
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

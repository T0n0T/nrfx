/**
 * @file drv_pm.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-20
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "drv_pm.h"
#include "bsp.h"
#include "nrf_pwr_mgmt.h"
#include <board.h>

static void sleep(struct rt_pm *pm, uint8_t mode)
{
    switch (mode) {
        case PM_SLEEP_MODE_NONE:
            break;

        case PM_SLEEP_MODE_IDLE:
            __WFE();
            break;

        case PM_SLEEP_MODE_LIGHT:
            do {
                __WFE();
            } while (0 == flag);
            break;

        case PM_SLEEP_MODE_DEEP:
            do {
                __WFE();
            } while (0 == flag);
            break;

        case PM_SLEEP_MODE_STANDBY:
            /* Enter STANDBY mode */

            break;

        case PM_SLEEP_MODE_SHUTDOWN:
            /* Enter SHUTDOWNN mode */
            bsp_uninit();
            set_sleep_exit_pin();
            nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_STAY_IN_SYSOFF);
            break;

        default:
            RT_ASSERT(0);
            break;
    }
}

static void run(struct rt_pm *pm, uint8_t mode)
{
    static uint8_t last_mode;
    static char *run_str[] = PM_RUN_MODE_NAMES;

    if (mode == last_mode)
        return;
    last_mode = mode;

    /* 根据RUN模式切换时钟频率(HSI) */
    switch (mode) {
        case PM_RUN_MODE_HIGH_SPEED:
        case PM_RUN_MODE_NORMAL_SPEED:
            break;
        case PM_RUN_MODE_MEDIUM_SPEED:
            break;
        case PM_RUN_MODE_LOW_SPEED:
            break;
        default:
            break;
    }
}

/**
 * This function start the timer of pm
 *
 * @param pm Pointer to power manage structure
 * @param timeout How many OS Ticks that MCU can sleep
 */
static void pm_timer_start(struct rt_pm *pm, rt_uint32_t timeout)
{
    RT_ASSERT(pm != RT_NULL);
    RT_ASSERT(timeout > 0);

    if (timeout != RT_TICK_MAX) {
        flag = 0;
        bsp_uninit();
        nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);
        nrf_rtc_int_disable(NRF_RTC1, NRF_RTC_INT_TICK_MASK);

        if (timeout > RTC_COUNTER_COUNTER_Msk) {
            timeout = RTC_COUNTER_COUNTER_Msk;
        }
        // OS tick is same as rtc tick
        nrf_rtc_cc_set(NRF_RTC1, 0, timeout * 1);
        nrf_rtc_task_trigger(NRF_RTC1, NRF_RTC_TASK_CLEAR);
        nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);
        nrf_rtc_int_enable(NRF_RTC1, NRF_RTC_INT_COMPARE0_MASK);
        __DSB();
    }
}

/**
 * This function stop the timer of pm
 *
 * @param pm Pointer to power manage structure
 */
static void pm_timer_stop(struct rt_pm *pm)
{
    RT_ASSERT(pm != RT_NULL);
    nrf_rtc_int_disable(NRF_RTC1, NRF_RTC_INT_COMPARE0_MASK);
    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);
    bsp_init();
    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_TICK);
    nrf_rtc_int_enable(NRF_RTC1, NRF_RTC_INT_TICK_MASK);
    NVIC_ClearPendingIRQ(RTC1_IRQn);
}

/**
 * This function calculate how many OS Ticks that MCU have suspended
 *
 * @param pm Pointer to power manage structure
 *
 * @return OS Ticks
 */
static rt_tick_t pm_timer_get_tick(struct rt_pm *pm)
{
    rt_uint32_t timer_tick;

    RT_ASSERT(pm != RT_NULL);

    return nrf_rtc_counter_get(NRF_RTC1);
}

/**
 * This function initialize the power manager
 */
int drv_pm_hw_init(void)
{
    static const struct rt_pm_ops _ops = {
        sleep,
        run,
        pm_timer_start,
        pm_timer_stop,
        pm_timer_get_tick,
    };

    rt_uint8_t timer_mask = 0;

    /* initialize timer mask */
    timer_mask |= 1UL << PM_SLEEP_MODE_LIGHT;
    timer_mask |= 1UL << PM_SLEEP_MODE_DEEP;
    /* initialize system pm module */
    rt_system_pm_init(&_ops, timer_mask, RT_NULL);

    return 0;
}

INIT_BOARD_EXPORT(drv_pm_hw_init);
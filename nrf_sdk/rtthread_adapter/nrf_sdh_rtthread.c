/**
 * @file nrf_sdh_rtthread.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-19
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "nrf_sdh_rtthread.h"
#include "nrf_sdh.h"

#include "rtdevice.h"
#include "rtthread.h"

#define NRF_LOG_MODULE_NAME nrf_sdh_rtthread
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define NRF_BLE_RTTHREAD_SDH_TASK_STACK 1024

static uint8_t ble_stack[NRF_BLE_RTTHREAD_SDH_TASK_STACK];
static struct rt_thread softdevice_thread;  //!< Reference to SoftDevice FreeRTOS task.
static nrf_sdh_rtthread_hook_t m_task_hook; //!< A hook function run by the SoftDevice task before entering its loop.
static struct rt_completion m_completion;

void SD_EVT_IRQHandler(void)
{
    rt_completion_done(&m_completion);
    rt_thread_yield();
}

/* This function gets events from the SoftDevice and processes them. */
static void softdevice_task(void *p)
{
    NRF_LOG_DEBUG("Enter softdevice_task.");

    if (m_task_hook != NULL) {
        m_task_hook(p);
    }

    while (true) {
        nrf_sdh_evts_poll(); /* let the handlers run first, incase the EVENT occured before creating this task */
        rt_completion_wait(&m_completion, RT_WAITING_FOREVER);
    }
}

void nrf_sdh_rtthread_init(nrf_sdh_rtthread_hook_t hook_fn, void *p_context)
{
    NRF_LOG_DEBUG("Creating a SoftDevice task.");

    m_task_hook = hook_fn;

    if (rt_thread_init(&softdevice_thread,
                       "BLE",
                       softdevice_task,
                       p_context,
                       ble_stack,
                       sizeof(ble_stack),
                       16,
                       20) != RT_EOK) {
        NRF_LOG_ERROR("SoftDevice task not created.");
        return;
    }
    rt_thread_startup(&softdevice_thread);
}
/**
 * @file nrf_sdh_rtthread.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-19
 *
 * @copyright Copyright (c) 2023
 *
 */

typedef void (*nrf_sdh_rtthread_hook_t)(void *p_context);

/**@brief   Function for creating a task to retrieve SoftDevice events.
 * @param[in]   hook        Function to run in the SoftDevice FreeRTOS task,
 *                          before entering the task loop.
 * @param[in]   p_context   Parameter for the function @p hook.
 */
void nrf_sdh_rtthread_init(nrf_sdh_rtthread_hook_t hook_fn, void *p_context);

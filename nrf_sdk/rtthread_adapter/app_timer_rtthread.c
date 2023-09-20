/**
 * Copyright (c) 2014 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(APP_TIMER)
#include "rtthread.h"

#include "app_timer.h"
#include <stdlib.h>
#include <string.h>
#include "nrf.h"
#include "app_error.h"

/**
 * Note that this implementation is made only for enable SDK components which interacts with app_timer to work with FreeRTOS.
 * It is more suitable to use native FreeRTOS timer for other purposes.
 */
/* Check if RTC FreeRTOS version is used */

/**
 * @brief Waiting time for the timer queue
 *
 * Number of system ticks to wait for the timer queue to put the message.
 * It is strongly recommended to set this to the value bigger than 1.
 * In other case if timer message queue is full - any operation on timer may fail.
 * @note
 * Timer functions called from interrupt context would never wait.
 */

/**@brief This structure keeps information about osTimer.*/
typedef struct
{
    void *argument;
    rt_timer_t osHandle;
    app_timer_timeout_handler_t func;
    /**
     * This member is to make sure that timer function is only called if timer is running.
     * FreeRTOS may have timer running even after stop function is called,
     * because it processes commands in Timer task and stopping function only puts command into the queue. */
    bool active;
    bool single_shot;
} app_timer_info_t;

/* Check if app_timer_t variable type can held our app_timer_info_t structure */
STATIC_ASSERT(sizeof(app_timer_info_t) <= sizeof(app_timer_t));

/**
 * @brief Internal callback function for the system timer
 *
 * Internal function that is called from the system timer.
 * It gets our parameter from timer data and sends it to user function.
 * @param[in] xTimer Timer handler
 */
static void app_timer_callback(void *parameter)
{
    app_timer_info_t *pinfo = (app_timer_info_t *)parameter;
    ASSERT(pinfo->func != NULL);

    if (pinfo->active) {
        pinfo->active = (pinfo->single_shot) ? false : true;
        pinfo->func(pinfo->argument);
    }
}

uint32_t app_timer_init(void)
{
    return NRF_SUCCESS;
}

uint32_t app_timer_create(app_timer_id_t const *p_timer_id,
                          app_timer_mode_t mode,
                          app_timer_timeout_handler_t timeout_handler)
{
    app_timer_info_t *pinfo = (app_timer_info_t *)(*p_timer_id);
    uint32_t err_code       = NRF_SUCCESS;
    uint8_t timer_mode;

    if ((timeout_handler == NULL) || (p_timer_id == NULL)) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (pinfo->active) {
        return NRF_ERROR_INVALID_STATE;
    }

    if (pinfo->osHandle == NULL) {
        /* New timer is created */
        memset(pinfo, 0, sizeof(app_timer_info_t));

        timer_mode         = (mode == APP_TIMER_MODE_SINGLE_SHOT) ? RT_TIMER_FLAG_ONE_SHOT : RT_TIMER_FLAG_PERIODIC;
        pinfo->single_shot = (mode == APP_TIMER_MODE_SINGLE_SHOT);
        pinfo->func        = timeout_handler;
        pinfo->osHandle    = rt_timer_create(" ", app_timer_callback, (void *)pinfo, 1000, timer_mode);

        if (pinfo->osHandle == NULL)
            err_code = NRF_ERROR_NULL;
    } else {
        return NRF_ERROR_INVALID_STATE;
    }

    return err_code;
}

uint32_t app_timer_start(app_timer_id_t timer_id, uint32_t timeout_ticks, void *p_context)
{
    app_timer_info_t *pinfo = (app_timer_info_t *)(timer_id);
    rt_timer_t timer        = pinfo->osHandle;

    if (timer == NULL) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (pinfo->active) {
        // Timer already running - exit silently
        return NRF_SUCCESS;
    }

    pinfo->argument = p_context;

    if (rt_timer_control(timer, RT_TIMER_CTRL_SET_TIME, &timeout_ticks) != RT_EOK) {
        return NRF_ERROR_NO_MEM;
    }
    if (rt_timer_start(timer) != RT_EOK) {
        return NRF_ERROR_NO_MEM;
    }
    pinfo->active = true;
    return NRF_SUCCESS;
}

uint32_t app_timer_stop(app_timer_id_t timer_id)
{
    app_timer_info_t *pinfo = (app_timer_info_t *)(timer_id);
    rt_timer_t timer        = pinfo->osHandle;
    if (timer == NULL) {
        return NRF_ERROR_INVALID_STATE;
    }

    if (rt_timer_stop(timer) != RT_EOK) {
        return NRF_ERROR_NO_MEM;
    }
    // if (rt_timer_delete(timer) != RT_EOK) {
    //     return NRF_ERROR_INTERNAL;
    // }
    pinfo->active = false;
    return NRF_SUCCESS;
}
#endif // NRF_MODULE_ENABLED(APP_TIMER)

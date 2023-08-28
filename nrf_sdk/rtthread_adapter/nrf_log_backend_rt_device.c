/**
 * Copyright (c) 2016 - 2021, Nordic Semiconductor ASA
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

/**
 * @brief rt-device log兼容输出
 * 在是能rt-device的log输出后，此处设备和控制台等其他输出设备不能相同，如果相同需要先屏蔽该设备的log，否则会出现递归打印
 * 如使用下面宏定义，直接将某模块的log等级设为off
 * #define NRF_LOG_FILTER 0
 */
#if NRF_MODULE_ENABLED(NRF_LOG) && NRF_MODULE_ENABLED(NRF_LOG_BACKEND_RT_DEVICE)
#include "nrf_log_backend_rt_device.h"
#include "nrf_log_backend_serial.h"
#include "nrf_log_internal.h"
#include <rtdevice.h>
#include "app_error.h"

const nrf_log_backend_api_t nrf_log_backend_rt_device_api;
NRF_LOG_BACKEND_DEF(rt_device_log_backend, nrf_log_backend_rt_device_api, NULL);
static uint8_t m_string_buff[NRF_LOG_BACKEND_RT_DEVICE_TEMP_BUFFER_SIZE];
#define RT_DEVICE_LOG_NAME "uart0"
rt_device_t dev = RT_NULL;

void nrf_log_backend_rt_device_init(void)
{
    int32_t backend_id = -1;
    (void)backend_id;
    dev = rt_device_find(RT_DEVICE_LOG_NAME);
    if (dev == RT_NULL) {
        printf("can't find rt-device %s for nrf_log module\n", RT_DEVICE_LOG_NAME);
        return;
    }
    backend_id = nrf_log_backend_add(&rt_device_log_backend, NRF_LOG_SEVERITY_DEBUG);
    ASSERT(backend_id > 0);
    nrf_log_backend_enable(&rt_device_log_backend);
}

void serial_tx(void const *p_context, char const *p_buffer, size_t len)
{
    if (rt_device_write(dev, 0, (void *)p_buffer, len) != len) {
        printf("rt-device %s transmit log failed\n", RT_DEVICE_LOG_NAME);
    }
}

static void nrf_log_backend_rt_device_put(nrf_log_backend_t const *p_backend,
                                          nrf_log_entry_t *p_msg)
{
    nrf_log_backend_serial_put(p_backend, p_msg, m_string_buff, NRF_LOG_BACKEND_RTT_TEMP_BUFFER_SIZE, serial_tx);
}

static void nrf_log_backend_rt_device_flush(nrf_log_backend_t const *p_backend)
{
}

static void nrf_log_backend_rt_device_panic_set(nrf_log_backend_t const *p_backend)
{
}

const nrf_log_backend_api_t nrf_log_backend_rt_device_api = {
    .put       = nrf_log_backend_rt_device_put,
    .flush     = nrf_log_backend_rt_device_flush,
    .panic_set = nrf_log_backend_rt_device_panic_set,
};
#endif // NRF_MODULE_ENABLED(NRF_LOG) && NRF_MODULE_ENABLED(NRF_LOG_BACKEND_rt_device)

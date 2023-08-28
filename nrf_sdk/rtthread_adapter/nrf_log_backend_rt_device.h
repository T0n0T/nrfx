/**
 * @file nrf_log_backend_rt_device.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-28
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef NRF_LOG_BACKEND_RT_DEVICE_H
#define NRF_LOG_BACKEND_RT_DEVICE_H

#include "nrf_log_backend_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const nrf_log_backend_api_t nrf_log_backend_rt_device_api;

typedef struct {
    nrf_log_backend_t backend;
} nrf_log_backend_rt_device_t;

#define NRF_LOG_BACKEND_UART_DEF(_name) \
    NRF_LOG_BACKEND_DEF(_name, nrf_log_backend_rt_device_api, NULL)

void nrf_log_backend_rt_device_init(void);

#ifdef __cplusplus
}
#endif

#endif // NRF_LOG_BACKEND_RT_DEVICE_H
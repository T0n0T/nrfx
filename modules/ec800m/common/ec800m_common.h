/**
 * @file ec800m_common.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-03-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef EC800M_COMMON_H
#define EC800M_COMMON_H

#include "nrfx_gpiote.h"

typedef enum {
    EC800M_TASK_POWERON = 1,
    EC800M_TASK_POWEROFF,
    EC800M_TASK_POWERLOW,
    EC800M_TASK_SIGNAL_CHECK,
    EC800M_TASK_GNSS_OPEN,
    EC800M_TASK_GNSS_GET,
} ec800m_comm_task_t;

/**
 * @brief turn on ec800m
 * @note block
 * @param dev 
 */
void ec800m_power_on(ec800m_t* dev);

/**
 * @brief turn off ec800m
 * @note no block
 * @param dev 
 */
void ec800m_power_off(ec800m_t* dev);

/**
 * @brief use low-power
 * @note no block
 * @param dev 
 */
void ec800m_power_low(ec800m_t* dev);

/**
 * @brief check signal
 * @note no block
 * @param dev
 */
void ec800m_signal_check(ec800m_t* dev);

/**
 * @brief check and open gnss
 * @note no block
 * @todo maybe block-mode is need,because gnss can't be used as fast as ec800m turning on.
 * @param dev
 */
void ec800m_gnss_open(ec800m_t* dev);

/**
 * @brief get gnss info
 * @note block
 * @param dev
 * @return gps_info_t
 */
gps_info_t ec800m_gnss_get(ec800m_t* dev);

#endif
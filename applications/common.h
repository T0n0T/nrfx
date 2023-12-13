/**
 * @file common.h
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-10-25
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"

#include "ble_advdata.h"
#include "ble_advertising.h"

#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"
#include "app_timer.h"

#include "blood.h"
#include "bsp.h"
#include "app.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define DEVICE_NAME                      "TICKLESS" /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME                "TEST"     /**< Manufacturer. Will be passed to Device Information Service. */

#define APP_BLE_OBSERVER_PRIO            3 /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG             1 /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_ADV_INTERVAL                 300   /**< The advertising interval (in units of 0.625 ms. This value corresponds to 187.5 ms). */
#define APP_ADV_DURATION                 18000 /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define BATTERY_LEVEL_MEAS_INTERVAL      2000 /**< Battery level measurement interval (ms). */
#define MIN_BATTERY_LEVEL                81   /**< Minimum simulated battery level. */
#define MAX_BATTERY_LEVEL                100  /**< Maximum simulated battery level. */
#define BATTERY_LEVEL_INCREMENT          1    /**< Increment between each simulated battery level measurement. */

#define HEART_RATE_MEAS_INTERVAL         1000 /**< Heart rate measurement interval (ms). */
#define MIN_HEART_RATE                   140  /**< Minimum heart rate as returned by the simulated measurement function. */
#define MAX_HEART_RATE                   300  /**< Maximum heart rate as returned by the simulated measurement function. */
#define HEART_RATE_INCREMENT             10   /**< Value by which the heart rate is incremented/decremented for each call to the simulated measurement function. */

#define SENSOR_CONTACT_DETECTED_INTERVAL 5000 /**< Sensor Contact Detected toggle interval (ms). */

#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(400, UNIT_1_25_MS) /**< Minimum acceptable connection interval (0.4 seconds). */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(650, UNIT_1_25_MS) /**< Maximum acceptable connection interval (0.65 second). */
#define SLAVE_LATENCY                    0                                /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(4000, UNIT_10_MS)  /**< Connection supervisory time-out (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY   5000  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY    30000 /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT     3     /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                   1                    /**< Perform bonding. */
#define SEC_PARAM_MITM                   0                    /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                   0                    /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS               0                    /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES        BLE_GAP_IO_CAPS_NONE /**< No I/O capabilities. */
#define SEC_PARAM_OOB                    0                    /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE           7                    /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE           16                   /**< Maximum encryption key size. */

#define DEAD_BEEF                        0xDEADBEEF /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define OSTIMER_WAIT_FOR_QUEUE           2 /**< Number of ticks to wait for the timer queue to be ready */

BLE_ADVERTISING_DEF(m_advertising); /**< Advertising module instance. */

extern uint16_t m_ble_nus_max_data_len;
extern uint16_t m_conn_handle;

extern void dfu_service_init(void);
extern void nus_service_init(void);

#endif

/**
 * @file ble_common.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-19
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "rtthread.h"
// Log需要引用的头文件
#include "nrf_log.h"
// APP定时器需要引用的头文件
#include "app_timer.h"

// 广播需要引用的头文件
#include "ble_advdata.h"
#include "ble_advertising.h"

// SoftDevice handler configuration需要引用的头文件
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
// 排序写入模块需要引用的头文件
#include "nrf_ble_qwr.h"
// GATT需要引用的头文件
#include "nrf_ble_gatt.h"
// 连接参数协商需要引用的头文件
#include "ble_conn_params.h"

#define DEVICE_NAME                    "NRF_CTT"                        // 设备名称字符串
#define MIN_CONN_INTERVAL              MSEC_TO_UNITS(100, UNIT_1_25_MS) // 最小连接间隔 (0.1 秒)
#define MAX_CONN_INTERVAL              MSEC_TO_UNITS(200, UNIT_1_25_MS) // 最大连接间隔 (0.2 秒)
#define SLAVE_LATENCY                  0                                // 从机延迟
#define CONN_SUP_TIMEOUT               MSEC_TO_UNITS(4000, UNIT_10_MS)  // 监督超时(4 秒)
#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(5000)            // 定义首次调用sd_ble_gap_conn_param_update()函数更新连接参数延迟时间（5秒）
#define NEXT_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(30000)           // 定义每次调用sd_ble_gap_conn_param_update()函数更新连接参数的间隔时间（30秒）
#define MAX_CONN_PARAMS_UPDATE_COUNT   3                                // 定义放弃连接参数协商前尝试连接参数协商的最大次数（3次）

#define APP_ADV_INTERVAL               320 // 广播间隔 (200ms)，单位0.625 ms
#define APP_ADV_DURATION               0   // 广播持续时间，单位：10ms。设置为0表示不超时

#define APP_BLE_OBSERVER_PRIO          3 // 应用程序BLE事件监视者优先级，应用程序不能修改该数值
#define APP_BLE_CONN_CFG_TAG           1 // SoftDevice BLE配置标志

// 定义操作码长度
#define OPCODE_LENGTH 1
// 定义句柄长度
#define HANDLE_LENGTH 2

/** @brief 预定义宏 */
NRF_BLE_GATT_DEF(m_gatt);           // 定义名称为m_gatt的GATT模块实例
NRF_BLE_QWR_DEF(m_qwr);             // 定义一个名称为m_qwr的排队写入实例
BLE_ADVERTISING_DEF(m_advertising); // 定义名称为m_advertising的广播模块实例
typedef void (*ble_serv_init_fn)(void);

/**
 * @brief 引入ble服务
 */
extern uint16_t m_conn_handle;
extern uint16_t max_data_len;
extern ble_uuid_t ble_app_log;
#define BLE_UUID_SERVICE_TABLE \
    {                          \
        &ble_app_log,          \
    }

extern ble_serv_init_fn ble_app_dfu_init;
extern ble_serv_init_fn ble_app_log_init;
#define BLE_SERVICE_INIT_TABLE \
    {                          \
        &ble_app_log_init,     \
            &ble_app_dfu_init, \
    }
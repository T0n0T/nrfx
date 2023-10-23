/**
 * @file ble_app_log.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-20
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef BLE_APP_LOG_H__
#define BLE_APP_LOG_H__
#include <stdint.h>
#include <stdbool.h>
#include "sdk_config.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"
#include "ble_link_ctx_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// 定义日志透传服务实例，该实例完成2件事情
// 1：定义了static类型日志透传服务结构体变量，为透传服务结构体分配了内存
// 2：注册了LOG事件监视者，这使得日志透传程序模块可以接收BLE协议栈的事件，从而可以在ble_log_on_ble_evt()事件回调函数中处理自己感兴趣的事件
#define BLE_LOG_DEF(_name, _log_max_clients)                            \
    BLE_LINK_CTX_MANAGER_DEF(CONCAT_2(_name, _link_ctx_storage),        \
                             (_log_max_clients),                        \
                             sizeof(ble_log_client_context_t));         \
    static ble_log_t _name =                                            \
        {                                                               \
            .p_link_ctx_storage = &CONCAT_2(_name, _link_ctx_storage)}; \
    NRF_SDH_BLE_OBSERVER(_name##_obs,                                   \
                         BLE_NUS_BLE_OBSERVER_PRIO,                     \
                         ble_log_on_ble_evt,                            \
                         &_name)

// 定义日志透传服务128位UUID基数
#define LOG_BASE_UUID                                                                                      \
    {                                                                                                      \
        {                                                                                                  \
            0x40, 0xE3, 0x4A, 0x1D, 0xC2, 0x5F, 0xB0, 0x9C, 0xB7, 0x47, 0xE6, 0x43, 0x00, 0x00, 0x53, 0x86 \
        }                                                                                                  \
    }
// 定义服务和特征的16位UUID
#define BLE_UUID_LOG_SERVICE           0x000A // 日志透传服务16位UUID
#define BLE_UUID_LOG_TX_CHARACTERISTIC 0x000B // TX特征16位UUID
#define BLE_UUID_LOG_RX_CHARACTERISTIC 0x000C // RX特征16位UUID

// 定义日志透传事件类型，这是用户自己定义的，供应用程序使用
typedef enum {
    BLE_LOG_EVT_RX_DATA,      // 接收到新的数据
    BLE_LOG_EVT_TX_RDY,       // 准备就绪，可以发送新数据
    BLE_NUS_EVT_COMM_STARTED, // 通知已经使能
    BLE_NUS_EVT_COMM_STOPPED, // 通知已经禁止
} ble_log_evt_type_t;

/* Forward declaration of the ble_nus_t type. */
typedef struct ble_log_s ble_log_t;

// 日志透传服务BLE_NUS_EVT_RX_DATA事件数据结构体，该结构体用于当BLE_NUS_EVT_RX_DATA产生时将接收的数据信息传递给处理函数
typedef struct
{
    uint8_t const *p_data; // 指向存放接收数据的缓存
    uint16_t length;       // 接收的数据长度
} ble_log_evt_rx_data_t;

// 记录对端设备是否使能了RX特征的通知
typedef struct
{
    bool is_notification_enabled;
} ble_log_client_context_t;

// 日志透传服务事件结构体
typedef struct
{
    ble_log_evt_type_t type;              // 事件类型
    ble_log_t *p_log;                     // 指向日志透传实例的指针
    uint16_t conn_handle;                 // 连接句柄
    ble_log_client_context_t *p_link_ctx; // 指向link context
    union {
        ble_log_evt_rx_data_t rx_data; // BLE_NUS_EVT_RX_DATA事件数据
    } params;
} ble_log_evt_t;

// 定义最大传输数据长度（字节数）
#if defined(NRF_SDH_BLE_GATT_MAX_MTU_SIZE) && (NRF_SDH_BLE_GATT_MAX_MTU_SIZE != 0)
#define BLE_LOG_MAX_DATA_LEN (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH)
#else
#define BLE_LOG_MAX_DATA_LEN (BLE_GATT_MTU_SIZE_DEFAULT - OPCODE_LENGTH - HANDLE_LENGTH)
#warning NRF_SDH_BLE_GATT_MAX_MTU_SIZE is not defined.
#endif

// 定义函数指针类型ble_log_data_handler_t
typedef void (*ble_log_data_handler_t)(ble_log_evt_t *p_evt);

// 日志服务初始化结构体
typedef struct
{
    ble_log_data_handler_t data_handler; // 处理接收数据的事件句柄
} ble_log_init_t;

// 日志透传服务结构体，包含所需要的信息
struct ble_log_s {
    uint8_t uuid_type;                                 // UUID类型
    uint16_t service_handle;                           // 日志透传服务句柄（由协议栈提供）
    ble_gatts_char_handles_t tx_handles;               // TX特征句柄
    ble_gatts_char_handles_t rx_handles;               // RX特征句柄
    blcm_link_ctx_storage_t *const p_link_ctx_storage; // 指向存储所有当前连接及其context的“link context”的句柄的指针
    ble_log_data_handler_t data_handler;               // 处理接收数据的事件句柄
};

void ble_log_flush_process(void *p_context);
void ble_log_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);
uint32_t ble_log_init(ble_log_t *p_log, ble_log_init_t const *p_log_init);
uint32_t ble_log_data_send(ble_log_t *p_log,
                           uint8_t *p_data,
                           uint16_t *p_length,
                           uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif // BLE_MY_LOG_H__

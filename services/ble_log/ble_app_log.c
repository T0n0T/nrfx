/**
 * @file ble_app_log.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-19
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "sdk_common.h"
#include "ble_common.h"
#include "rtthread.h"
#include "rtdevice.h"
#if NRF_MODULE_ENABLED(BLE_LOG)
#include "ble_app_log.h"

#include <stdlib.h>
#include <string.h>
#include "app_error.h"
#include "ble_gatts.h"
#include "ble_srv_common.h"
#include "app_timer.h"
#include "nrf_log.h"
#define NRF_LOG_FILTER        4

#define UART_TX_BUF_SIZE      256                        // 日志发送缓存大小（字节数）
#define UART_RX_BUF_SIZE      256                        // 接收缓存大小（字节数）
#define LOG_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN // 日志透传服务UUID类型：厂商自定义UUID
APP_TIMER_DEF(log_timer);                                // 日志定时器ID
BLE_LOG_DEF(m_log, NRF_SDH_BLE_TOTAL_LINK_COUNT);        // 定义名称为m_log的日志透传服务实例

// 蓝牙日志初始化情况
static bool log_enabled = false;
static struct rt_ringbuffer log_ringbuffer;
static char log_buf[UART_TX_BUF_SIZE * 2];

void rt_hw_console_output(const char *str)
{
    if (log_enabled) {
        rt_ringbuffer_put_force(&log_ringbuffer, str, rt_strlen(str)); // 循环更新日志buff
    }
}

void ble_log_flush_process(void *p_context)
{
    // 判断日志使能
    if (log_enabled) {
        uint32_t err_code;
        static uint8_t data_array[UART_TX_BUF_SIZE];
        uint16_t data_len = 0;
        uint8_t *index    = 0;
        // 获取日志数据
        data_len = rt_ringbuffer_get(&log_ringbuffer, data_array, UART_TX_BUF_SIZE); // 获取日志数据
        index    = &data_array[0];
        // 接收日志数据，当接收的数据长度达到max_data_len或者接收到换行符后认为一包数据接收完成
        while (data_len > 0) {
            if (data_len >= max_data_len) {
                NRF_LOG_INFO("Ready to send data len %d over BLE LOG", max_data_len);
                // 日志接收的数据使用notify发送给BLE主机
                do {
                    err_code = ble_log_data_send(&m_log, index, &max_data_len, m_conn_handle);
                    if ((err_code != NRF_ERROR_INVALID_STATE) &&
                        (err_code != NRF_ERROR_RESOURCES) &&
                        (err_code != NRF_ERROR_NOT_FOUND)) {
                        APP_ERROR_CHECK(err_code);
                    }
                } while (err_code == NRF_ERROR_RESOURCES);
                index += max_data_len;
                data_len -= max_data_len;
            } else {
                NRF_LOG_INFO("Ready to send data len %d over BLE LOG", data_len);
                // 日志接收的数据使用notify发送给BLE主机
                do {
                    err_code = ble_log_data_send(&m_log, index, &data_len, m_conn_handle);
                    if ((err_code != NRF_ERROR_INVALID_STATE) &&
                        (err_code != NRF_ERROR_RESOURCES) &&
                        (err_code != NRF_ERROR_NOT_FOUND)) {
                        APP_ERROR_CHECK(err_code);
                    }
                } while (err_code == NRF_ERROR_RESOURCES);
                break;
            }
        }
    }
}

// 日志透传事件回调函数，日志初始化时注册
static void log_data_handler(ble_log_evt_t *p_evt)
{
    // 通知使能后才初始化日志
    if (p_evt->type == BLE_NUS_EVT_COMM_STARTED) {
        app_timer_start(log_timer, 500, NULL);
        log_enabled = true;
    }
    // 通知关闭后，关闭日志
    else if (p_evt->type == BLE_NUS_EVT_COMM_STOPPED) {
        app_timer_stop(log_timer);
        log_enabled = false;
    }
    // 判断事件类型:接收到新数据事件
    if ((p_evt->type == BLE_LOG_EVT_RX_DATA) && (log_enabled == true)) {
        // 处理接收的数据
    }
    // 判断事件类型:发送就绪事件，该事件在后面的试验会用到，当前我们在该事件中翻转指示灯D4的状态，指示该事件的产生
    if (p_evt->type == BLE_LOG_EVT_TX_RDY) {
    }
}

#define BLE_LOG_MAX_RX_CHAR_LEN BLE_LOG_MAX_DATA_LEN // RX特征最大长度（字节数）
#define BLE_LOG_MAX_TX_CHAR_LEN BLE_LOG_MAX_DATA_LEN // TX特征最大长度（字节数）

static void on_connect(ble_log_t *p_log, ble_evt_t const *p_ble_evt)
{
    ret_code_t err_code;
    ble_log_evt_t evt;
    ble_gatts_value_t gatts_val;
    uint8_t cccd_value[2];
    ble_log_client_context_t *p_client = NULL;

    err_code = blcm_link_ctx_get(p_log->p_link_ctx_storage,
                                 p_ble_evt->evt.gap_evt.conn_handle,
                                 (void *)&p_client);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gap_evt.conn_handle);
    }

    /* Check the hosts CCCD value to inform of readiness to send data using the RX characteristic */
    memset(&gatts_val, 0, sizeof(ble_gatts_value_t));
    gatts_val.p_value = cccd_value;
    gatts_val.len     = sizeof(cccd_value);
    gatts_val.offset  = 0;

    err_code = sd_ble_gatts_value_get(p_ble_evt->evt.gap_evt.conn_handle,
                                      p_log->rx_handles.cccd_handle,
                                      &gatts_val);

    if ((err_code == NRF_SUCCESS) &&
        (p_log->data_handler != NULL) &&
        ble_srv_is_notification_enabled(gatts_val.p_value)) {
        if (p_client != NULL) {
            p_client->is_notification_enabled = true;
        }

        memset(&evt, 0, sizeof(ble_log_evt_t));
        evt.type        = BLE_NUS_EVT_COMM_STARTED;
        evt.p_log       = p_log;
        evt.conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        evt.p_link_ctx  = p_client;

        p_log->data_handler(&evt);
    }
}

// SoftDevice提交的主机"write"事件处理函数
static void on_write(ble_log_t *p_log, ble_evt_t const *p_ble_evt)
{
    ret_code_t err_code;
    // 定义一个日志透传事件结构体变量，用于执行回调时传递参数
    ble_log_evt_t evt;
    ble_log_client_context_t *p_client;
    // 定义write事件结构体指针并指向GATT事件中的wirite
    ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    err_code = blcm_link_ctx_get(p_log->p_link_ctx_storage,
                                 p_ble_evt->evt.gatts_evt.conn_handle,
                                 (void *)&p_client);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gatts_evt.conn_handle);
    }

    // 清零evt
    memset(&evt, 0, sizeof(ble_log_evt_t));
    // 指向日志透传服务实例
    evt.p_log = p_log;
    // 设置连接句柄
    evt.conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
    evt.p_link_ctx  = p_client;
    // 是否是写用户配置
    if ((p_evt_write->handle == p_log->tx_handles.cccd_handle) &&
        (p_evt_write->len == 2)) {
        if (p_client != NULL) {
            // 判断主机是否已经对 CCCD 描述符进行了使能, 并且长度等于2，是则需要配置客户端用户配置
            if (ble_srv_is_notification_enabled(p_evt_write->data)) {
                p_client->is_notification_enabled = true;
                evt.type                          = BLE_NUS_EVT_COMM_STARTED;
            } else {
                p_client->is_notification_enabled = false;
                evt.type                          = BLE_NUS_EVT_COMM_STOPPED;
            }

            if (p_log->data_handler != NULL) {
                p_log->data_handler(&evt);
            }
        }
    }
    // 判断是主机写句柄，写RX特征值
    else if ((p_evt_write->handle == p_log->rx_handles.value_handle) &&
             (p_log->data_handler != NULL)) {
        // 设置事件类型
        evt.type = BLE_LOG_EVT_RX_DATA;
        // 设置数据长度
        evt.params.rx_data.p_data = p_evt_write->data;
        // 设置数据长度
        evt.params.rx_data.length = p_evt_write->len;
        // 执行回调
        p_log->data_handler(&evt);
    } else {
        // 该事件和日志透传服务无关，忽略
    }
}

// SoftDevice提交的"BLE_GATTS_EVT_HVN_TX_COMPLETE"事件处理函数，表示已经发送数据
static void on_hvx_tx_complete(ble_log_t *p_log, ble_evt_t const *p_ble_evt)
{
    ret_code_t err_code;
    // 定义一个日志透传事件结构体变量evt，用于执行回调时传递参数
    ble_log_evt_t evt;

    ble_log_client_context_t *p_client;

    err_code = blcm_link_ctx_get(p_log->p_link_ctx_storage,
                                 p_ble_evt->evt.gatts_evt.conn_handle,
                                 (void *)&p_client);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gatts_evt.conn_handle);
    }

    // 通知使能的情况下才会执行回调
    if (p_client->is_notification_enabled) {
        // 清零evt
        memset(&evt, 0, sizeof(ble_log_evt_t));
        // 设置事件类型
        evt.type = BLE_LOG_EVT_TX_RDY;
        // 指向日志透传服务实例
        evt.p_log = p_log;
        // 设置连接句柄
        evt.conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
        // 指向link context
        evt.p_link_ctx = p_client;
        // 执行回调
        p_log->data_handler(&evt);
    }
}

// 日志透传服务BLE事件监视者的事件回调函数
void ble_log_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
    // 检查参数是否有效
    if ((p_context == NULL) || (p_ble_evt == NULL)) {
        return;
    }
    // 定义一个日志透传结构体指针并指向日志透传结构体
    ble_log_t *p_log = (ble_log_t *)p_context;
    // 判断事件类型
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED: // 连接建立事件
            on_connect(p_log, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE: // 写事件
                                  // 处理写事件
            on_write(p_log, p_ble_evt);
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE: // softdevice的TX就绪
                                            // 处理TX队列中的信息
            on_hvx_tx_complete(p_log, p_ble_evt);
            break;

        default:
            break;
    }
}

// 初始化日志透传服务
uint32_t ble_log_init(ble_log_t *p_log, ble_log_init_t const *p_log_init)
{
    ret_code_t err_code;
    // 定义16位UUID结构体变量
    ble_uuid_t ble_uuid;
    // 定义128位UUID结构体变量，并初始化为日志透传服务UUID基数
    ble_uuid128_t nus_base_uuid = LOG_BASE_UUID;
    // 定义特征参数结构体变量
    ble_add_char_params_t add_char_params;
    // 检查指针是否为NULL
    VERIFY_PARAM_NOT_NULL(p_log);
    VERIFY_PARAM_NOT_NULL(p_log_init);

    // 拷贝日志透传服务初始化结构体中的事件句柄
    p_log->data_handler = p_log_init->data_handler;
    // 将自定义UUID基数添加到SoftDevice，并获取uuid_type
    err_code = sd_ble_uuid_vs_add(&nus_base_uuid, &p_log->uuid_type);
    VERIFY_SUCCESS(err_code);
    // UUID类型和数值赋值给ble_uuid变量
    ble_uuid.type = p_log->uuid_type;
    ble_uuid.uuid = BLE_UUID_LOG_SERVICE;
    // 添加日志透传服务, 获得句柄
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_log->service_handle);
    VERIFY_SUCCESS(err_code);
    /*---------------------以下代码添加RX特征--------------------*/
    // 添加RX特征
    // 配置参数之前先清零add_char_params
    memset(&add_char_params, 0, sizeof(add_char_params));
    // RX特征的UUID
    add_char_params.uuid = BLE_UUID_LOG_RX_CHARACTERISTIC;
    // RX特征的UUID类型
    add_char_params.uuid_type = p_log->uuid_type;
    // 设置RX特征值的最大长度
    add_char_params.max_len = BLE_LOG_MAX_RX_CHAR_LEN;
    // 设置RX特征值的初始长度
    add_char_params.init_len = sizeof(uint8_t);
    // 设置RX的特征值长度为可变长度
    add_char_params.is_var_len = true;
    // 设置RX特征的性质支持写
    add_char_params.char_props.write = 1;
    // 设置RX特征的性质支持无响应写
    add_char_params.char_props.write_wo_resp = 1;
    // 设置读/写RX特征值的安全需求：无安全性
    add_char_params.read_access  = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;
    // 为日志透传服务添加RX特征
    err_code = characteristic_add(p_log->service_handle, &add_char_params, &p_log->rx_handles);
    // 检查返回的错误代码
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    /*---------------------添加RX特征-END------------------------*/

    /*---------------------以下代码添加TX特征--------------------*/
    // 添加TX特征
    // 配置参数之前先清零add_char_params
    memset(&add_char_params, 0, sizeof(add_char_params));
    // TX特征的UUID
    add_char_params.uuid = BLE_UUID_LOG_TX_CHARACTERISTIC;
    // TX特征的UUID类型
    add_char_params.uuid_type = p_log->uuid_type;
    // 设置TX特征值的最大长度
    add_char_params.max_len = BLE_LOG_MAX_TX_CHAR_LEN;
    // 设置TX特征值的初始长度
    add_char_params.init_len = sizeof(uint8_t);
    // 设置TX的特征值长度为可变长度
    add_char_params.is_var_len = true;
    // 设置TX特征的性质：支持通知
    add_char_params.char_props.notify = 1;
    // 设置读/写RX特征值的安全需求：无安全性
    add_char_params.read_access       = SEC_OPEN;
    add_char_params.write_access      = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;
    // 为日志透传服务添加TX特征
    return characteristic_add(p_log->service_handle, &add_char_params, &p_log->tx_handles);
    /*---------------------添加TX特征-END------------------------*/
}

uint32_t ble_log_data_send(ble_log_t *p_log,
                           uint8_t *p_data,
                           uint16_t *p_length,
                           uint16_t conn_handle)
{
    ret_code_t err_code;
    ble_gatts_hvx_params_t hvx_params;
    ble_log_client_context_t *p_client;
    // 验证p_log没有指向NULL
    VERIFY_PARAM_NOT_NULL(p_log);

    err_code = blcm_link_ctx_get(p_log->p_link_ctx_storage, conn_handle, (void *)&p_client);
    VERIFY_SUCCESS(err_code);

    // 如果连接句柄无效，表示没有和主机建立连接，返回NRF_ERROR_NOT_FOUND
    if ((conn_handle == BLE_CONN_HANDLE_INVALID) || (p_client == NULL)) {
        return NRF_ERROR_NOT_FOUND;
    }
    // 如果通知没有使能，返回状态无效的错误代码
    if (!p_client->is_notification_enabled) {
        return NRF_ERROR_INVALID_STATE;
    }
    // 如果数据长度超限，返回参数无效的错误代码
    if (*p_length > BLE_LOG_MAX_DATA_LEN) {
        return NRF_ERROR_INVALID_PARAM;
    }
    // 设置之前先清零hvx_params
    memset(&hvx_params, 0, sizeof(hvx_params));
    // TX特征值句柄
    hvx_params.handle = p_log->tx_handles.value_handle;
    // 发送的数据
    hvx_params.p_data = p_data;
    // 发送的数据长度
    hvx_params.p_len = p_length;
    // 类型为通知
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    // 发送TX特征值通知
    return sd_ble_gatts_hvx(conn_handle, &hvx_params);
}

static const void service_init(void)
{
    ble_log_init_t log_init = {0};
    rt_ringbuffer_init(&log_ringbuffer, log_buf, sizeof(log_buf));
    log_init.data_handler = log_data_handler;
    APP_ERROR_CHECK(ble_log_init(&m_log, &log_init));
    
    app_timer_create(&log_timer, APP_TIMER_MODE_REPEATED, ble_log_flush_process);
}

ble_serv_init_fn ble_app_log_init = service_init;
ble_uuid_t ble_app_log            = {
               .uuid = BLE_UUID_LOG_SERVICE,
               .type = LOG_SERVICE_UUID_TYPE,
};

#endif

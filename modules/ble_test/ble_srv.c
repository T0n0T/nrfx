/****************************************Copyright (c)************************************************
**                                      [艾克姆科技]
**                                        IIKMSIK
**                            官方店铺：https://acmemcu.taobao.com
**                            官方论坛：http://www.e930bbs.com
**
**--------------File Info-----------------------------------------------------------------------------
** File name         : main.c
** Last modified Date: 2020-9-23
** Last Version      :
** Descriptions      : 使用的SDK版本-SDK_17.0.2
**
**----------------------------------------------------------------------------------------------------
** Created by        : [艾克姆]
** Created date      : 2018-12-24
** Version           : 1.0
** Descriptions      : 实现心率Profile
**---------------------------------------------------------------------------------------------------*/
// 引用的C库头文件
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
// Log需要引用的头文件
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

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
// 广播需要引用的头文件
#include "ble_advdata.h"
#include "ble_advertising.h"
// 引用心率服务和设备信息服务头文件
#include "my_ble_hrs.h"
#include "my_ble_dis.h"

#define MANUFACTURER_NAME              "IKMSIK"                         // 制造商名称字符串
#define DEVICE_NAME                    "HeartSensor"                    // 设备名称字符串
#define MIN_CONN_INTERVAL              MSEC_TO_UNITS(100, UNIT_1_25_MS) // 最小连接间隔 (0.1 秒)
#define MAX_CONN_INTERVAL              MSEC_TO_UNITS(200, UNIT_1_25_MS) // 最大连接间隔 (0.2 秒)
#define SLAVE_LATENCY                  0                                // 从机延迟
#define CONN_SUP_TIMEOUT               MSEC_TO_UNITS(4000, UNIT_10_MS)  // 监督超时(4 秒)
#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(5000)            // 定义首次调用sd_ble_gap_conn_param_update()函数更新连接参数延迟时间（5秒）
#define NEXT_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(30000)           // 定义每次调用sd_ble_gap_conn_param_update()函数更新连接参数的间隔时间（30秒）
#define MAX_CONN_PARAMS_UPDATE_COUNT   3                                // 定义放弃连接参数协商前尝试连接参数协商的最大次数（3次）

#define APP_ADV_INTERVAL               300 // 广播间隔 (187.5 ms)，单位0.625 ms
#define APP_ADV_DURATION               0   // 广播持续时间，单位：10ms。设置为0表示不超时

#define APP_BLE_OBSERVER_PRIO          3 // 应用程序BLE事件监视者优先级，应用程序不能修改该数值
#define APP_BLE_CONN_CFG_TAG           1 // SoftDevice BLE配置标志

// 用于stack dump的错误代码，可以用于栈回退时确定堆栈位置
#define DEAD_BEEF 0xDEADBEEF

NRF_BLE_GATT_DEF(m_gatt);           // 定义名称为m_gatt的GATT模块实例
NRF_BLE_QWR_DEF(m_qwr);             // 定义一个名称为m_qwr的排队写入实例
BLE_ADVERTISING_DEF(m_advertising); // 定义名称为m_advertising的广播模块实例

// 该变量用于保存连接句柄，初始值设置为无连接
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

// 心率测量APP定时器（m_heart_rate_timer_id）超时时间：1秒
#define HEART_RATE_INTERVAL APP_TIMER_TICKS(1000)
// 传感器接触状态更新APP定时器（m_sensor_contact_update_timer_id）超时时间：5秒
#define SENSOR_CONTACT_UPDATE_INTERVAL APP_TIMER_TICKS(5000)
// 用于stack dump的错误代码，可以用于栈回退时确定堆栈位置
#define DEAD_BEEF 0xDEADBEEF

// 定义心率测量APP定时器m_heart_rate_timer_id
APP_TIMER_DEF(m_heart_rate_timer_id);
// 定义传感器接触状态更新APP定时器m_sensor_contact_update_timer_id
APP_TIMER_DEF(m_sensor_contact_update_timer_id);

// 定义心率服务实例
BLE_HRS_DEF(m_hrs);

// 定义服务UUID列表
static ble_uuid_t m_adv_uuids[] =
    {
        {BLE_UUID_HEART_RATE_SERVICE, BLE_UUID_TYPE_BLE},        // 心率服务UUID
        {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE} // 设备信息服务UUID
};

// GAP参数初始化，该函数配置需要的GAP参数，包括设备名称，外观特征、首选连接参数
static void gap_params_init(void)
{
    ret_code_t err_code;
    // 定义连接参数结构体变量
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;
    // 设置GAP的安全模式
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    // 设置GAP设备名称
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);

    // 设置外观特征:心率腕带
    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_HEART_RATE_SENSOR_HEART_RATE_BELT);
    APP_ERROR_CHECK(err_code);

    // 设置首选连接参数，设置前先清零gap_conn_params
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL; // 最小连接间隔
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL; // 最小连接间隔
    gap_conn_params.slave_latency     = SLAVE_LATENCY;     // 从机延迟
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;  // 监督超时
    // 调用协议栈API sd_ble_gap_ppcp_set配置GAP参数
    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}
// 初始化GATT程序模块
static void gatt_init(void)
{
    // 初始化GATT程序模块
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
}

// 排队写入事件处理函数，用于处理排队写入模块的错误
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    // 检查错误代码
    APP_ERROR_HANDLER(nrf_error);
}

// 服务初始化，包含初始化排队写入模块和初始化应用程序使用的服务
static void services_init(void)
{
    ret_code_t err_code;
    // 定义心率服务初始化结构体
    ble_hrs_init_t hrs_init;
    // 定义设备信息服务初始化结构体
    ble_dis_init_t dis_init;
    uint8_t body_sensor_location;
    // 定义排队写入初始化结构体变量
    nrf_ble_qwr_init_t qwr_init = {0};

    // 排队写入事件处理函数
    qwr_init.error_handler = nrf_qwr_error_handler;
    // 初始化排队写入模块
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    // 检查函数返回值
    APP_ERROR_CHECK(err_code);

    /*------------------以下代码初始化心率服务------------------*/
    // 传感器身体测点位置：手腕，因为心率服务初始化结构体中使用的是指针，因此要先定义一个变量，之后将该
    // 变量的地址赋值给心率服务初始化结构体中的p_body_sensor_location
    body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_WRIST;
    // 清零心率初始化结构体
    memset(&hrs_init, 0, sizeof(hrs_init));
    // 心率服务事件回调函数设置为NULL
    hrs_init.evt_handler = NULL;
    // 支持传感器接触检测
    hrs_init.is_sensor_contact_supported = true;
    // 设置传感器佩戴位置
    hrs_init.p_body_sensor_location = &body_sensor_location;

    // 设置心率服务的安全等级：无安全性
    hrs_init.hrm_cccd_wr_sec = SEC_OPEN;
    hrs_init.bsl_rd_sec      = SEC_OPEN;
    // 初始化心率服务
    err_code = ble_hrs_init(&m_hrs, &hrs_init);
    APP_ERROR_CHECK(err_code);
    /*--------------------初始化心率服务-END-------------------*/

    /*------------------以下代码初始化设备信息服务-------------*/
    // 清零设备信息初始化结构体
    memset(&dis_init, 0, sizeof(dis_init));

    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, (char *)MANUFACTURER_NAME);
    // 设置设备信息服务的安全等级：无安全性
    dis_init.dis_char_rd_sec = SEC_OPEN;
    // 初始化设备信息服务
    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);
    /*------------------初始化设备信息服务-END-----------------*/
}

// 连接参数协商模块事件处理函数
static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)
{
    ret_code_t err_code;
    // 判断事件类型，根据事件类型执行动作
    // 连接参数协商失败
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED) {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
    // 连接参数协商成功
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_SUCCEEDED) {
        // 功能代码;
    }
}

// 连接参数协商模块错误处理事件，参数nrf_error包含了错误代码，通过nrf_error可以分析错误信息
static void conn_params_error_handler(uint32_t nrf_error)
{
    // 检查错误代码
    APP_ERROR_HANDLER(nrf_error);
}

// 连接参数协商模块初始化
static void conn_params_init(void)
{
    ret_code_t err_code;
    // 定义连接参数协商模块初始化结构体
    ble_conn_params_init_t cp_init;
    // 配置之前先清零
    memset(&cp_init, 0, sizeof(cp_init));
    // 设置为NULL，从主机获取连接参数
    cp_init.p_conn_params = NULL;
    // 连接或启动通知到首次发起连接参数更新请求之间的时间设置为5秒
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    // 每次调用sd_ble_gap_conn_param_update()函数发起连接参数更新请求的之间的间隔时间设置为：30秒
    cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
    // 放弃连接参数协商前尝试连接参数协商的最大次数设置为：3次
    cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    // 连接参数更新从连接事件开始计时
    cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    // 连接参数更新失败不断开连接
    cp_init.disconnect_on_fail = false;
    // 注册连接参数更新事件句柄
    cp_init.evt_handler = on_conn_params_evt;
    // 注册连接参数更新错误事件句柄
    cp_init.error_handler = conn_params_error_handler;
    // 调用库函数（以连接参数更新初始化结构体为输入参数）初始化连接参数协商模块
    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

// 广播事件处理函数
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;
    // 判断广播事件类型
    switch (ble_adv_evt) {
            // 快速广播启动事件：快速广播启动后会产生该事件
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("Fast advertising.");
            // 设置广播指示灯为正在广播（D1指示灯闪烁）
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        // 广播IDLE事件：广播超时后会产生该事件
        case BLE_ADV_EVT_IDLE:
            // 设置广播指示灯为广播停止（D1指示灯熄灭）
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            break;
    }
}
// 广播初始化
static void advertising_init(void)
{
    ret_code_t err_code;
    // 定义广播初始化配置结构体变量
    ble_advertising_init_t init;
    // 配置之前先清零
    memset(&init, 0, sizeof(init));
    // 设备名称类型：全称
    init.advdata.name_type = BLE_ADVDATA_FULL_NAME;
    // 是否包含外观：包含
    init.advdata.include_appearance = true;
    // Flag:一般可发现模式，不支持BR/EDR
    init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    // 广播中包含服务的UUID
    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids  = m_adv_uuids;
    // 设置广播模式为快速广播
    init.config.ble_adv_fast_enabled = true;
    // 设置广播间隔和广播持续时间
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    // 广播事件回调函数
    init.evt_handler = on_adv_evt;
    // 初始化广播
    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);
    // 设置广播配置标记。APP_BLE_CONN_CFG_TAG是用于跟踪广播配置的标记，这是为未来预留的一个参数，在将来的SoftDevice版本中，
    // 可以使用sd_ble_gap_adv_set_configure()配置新的广播配置
    // 当前SoftDevice版本（S132 V7.2.0版本）支持的最大广播集数量为1，因此APP_BLE_CONN_CFG_TAG只能写1。
    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

// BLE事件处理函数
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    ret_code_t err_code = NRF_SUCCESS;
    // 判断BLE事件类型，根据事件类型执行相应操作
    switch (p_ble_evt->header.evt_id) {
            // 断开连接事件
        case BLE_GAP_EVT_DISCONNECTED:
            // 打印提示信息
            NRF_LOG_INFO("Disconnected.");
            break;

        // 连接事件
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected.");
            // 设置指示灯状态为连接状态，即指示灯D1常亮
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            // 保存连接句柄
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            // 将连接句柄分配给排队写入实例，分配后排队写入实例和该连接关联，这样，当有多个连接的时候，通过关联不同的排队写入实例，很方便单独处理各个连接
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        // PHY更新事件
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
                {
                    .rx_phys = BLE_GAP_PHY_AUTO,
                    .tx_phys = BLE_GAP_PHY_AUTO,
                };
            // 响应PHY更新规程
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        // GATT客户端超时事件
        case BLE_GATTC_EVT_TIMEOUT:
            NRF_LOG_DEBUG("GATT Client Timeout.");
            // 断开当前连接
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        // GATT服务器超时事件
        case BLE_GATTS_EVT_TIMEOUT:
            NRF_LOG_DEBUG("GATT Server Timeout.");
            // 断开当前连接
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            break;
    }
}

// 初始化BLE协议栈
static void ble_stack_init(void)
{
    ret_code_t err_code;
    // 请求使能SoftDevice，该函数中会根据sdk_config.h文件中低频时钟的设置来配置低频时钟
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // 定义保存应用程序RAM起始地址的变量
    uint32_t ram_start = 0;
    // 使用sdk_config.h文件的默认参数配置协议栈，获取应用程序RAM起始地址，保存到变量ram_start
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // 使能BLE协议栈
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // 注册BLE事件回调函数
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}
// 初始化电源管理模块
static void power_management_init(void)
{
    ret_code_t err_code;
    // 初始化电源管理
    err_code = nrf_pwr_mgmt_init();
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
}
// 心率测量定时器事件回调函数
static void heart_rate_timeout_handler(void *p_context)
{
    ret_code_t err_code;
    uint16_t heart_rate;

    UNUSED_PARAMETER(p_context);
    // 心率值写死为：270bpm
    heart_rate = 270;
    // 发送心率测量值
    err_code = ble_hrs_heart_rate_measurement_send(&m_hrs, heart_rate);
    if ((err_code != NRF_SUCCESS) &&
        (err_code != NRF_ERROR_INVALID_STATE) &&
        (err_code != NRF_ERROR_RESOURCES) &&
        (err_code != NRF_ERROR_BUSY) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)) {
        APP_ERROR_HANDLER(err_code);
    }
}
// 传感器接触状态更新定时器事件回调函数
static void sensor_contact_update_timeout_handler(void *p_context)
{
    static bool sensor_contact_detected = false;

    UNUSED_PARAMETER(p_context);
    // 取反传感器接触状态
    sensor_contact_detected = !sensor_contact_detected;
    // 设置传感器接触状态
    ble_hrs_sensor_contact_detected_update(&m_hrs, sensor_contact_detected);
}

// 初始化指示灯
static void leds_init(void)
{
    ret_code_t err_code;
    // 初始化BSP指示灯
    err_code = bsp_init(BSP_INIT_LEDS, NULL);
    APP_ERROR_CHECK(err_code);
}
// 初始化APP定时器模块
static void timers_init(void)
{
    // 初始化APP定时器模块
    ret_code_t err_code = app_timer_init();
    // 检查返回值
    APP_ERROR_CHECK(err_code);

    // 创建APP定时器：心率测量定时器（m_heart_rate_timer_id），周期性定时器，超时时间1秒
    err_code = app_timer_create(&m_heart_rate_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                heart_rate_timeout_handler);
    APP_ERROR_CHECK(err_code);

    // 创建APP定时器：传感器接触状态更新定时器（m_sensor_contact_update_timer_id），周期性定时器，超时时间5秒
    err_code = app_timer_create(&m_sensor_contact_update_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                sensor_contact_update_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

static void log_init(void)
{
    // 初始化log程序模块
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    // 设置log输出终端（根据sdk_config.h中的配置设置输出终端为UART或者RTT）
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

// 空闲状态处理函数。如果没有挂起的日志操作，则睡眠直到下一个事件发生后唤醒系统
void idle_state_handle(void)
{
    // 处理挂起的log
    if (NRF_LOG_PROCESS() == false) {
        // 运行电源管理，该函数需要放到主循环里面执行
        nrf_pwr_mgmt_run();
    }
}
// 启动广播，该函数所用的模式必须和广播初始化中设置的广播模式一样
static void advertising_start(void)
{
    // 使用广播初始化中设置的广播模式启动广播
    ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
}
static void application_timers_start(void)
{
    ret_code_t err_code;
    // 启动心率测量定时器，超时时间1秒
    err_code = app_timer_start(m_heart_rate_timer_id, HEART_RATE_INTERVAL, NULL);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
    // 启动传感器接触状态更新定时器，超时时间5秒
    err_code = app_timer_start(m_sensor_contact_update_timer_id, SENSOR_CONTACT_UPDATE_INTERVAL, NULL);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
}
// 主函数
int ble_test(void)
{
    // 初始化log程序模块
    log_init();
    // 初始化APP定时器
    timers_init();
    // 出使唤按键和指示灯
    leds_init();
    // 初始化电源管理
    // power_management_init();
    // 初始化协议栈
    ble_stack_init();
    // 配置GAP参数
    gap_params_init();
    // 初始化GATT
    gatt_init();
    // 初始化广播
    advertising_init();
    // 初始化服务
    services_init();
    // 连接参数协商初始化
    conn_params_init();

    NRF_LOG_INFO("BLE HRS example started.");
    application_timers_start();
    // 启动广播
    advertising_start();
}

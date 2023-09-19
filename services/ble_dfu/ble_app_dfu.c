/**
 * @file ble_app_dfu.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-19
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "rtthread.h"
#include "nrf_pwr_mgmt.h"
// 引用的C库头文件
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
// Log需要引用的头文件
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
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
// 日志透传需要引用的头文件
#include "ble_app_log.h"
// DFU需要引用的头文件
#include "nrf_dfu_ble_svci_bond_sharing.h"
#include "nrf_svci_async_function.h"
#include "nrf_svci_async_handler.h"
#include "nrf_power.h"
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"

#define DEVICE_NAME                    "BLE_UartApp"                    // 设备名称字符串
#define UARTS_SERVICE_UUID_TYPE        BLE_UUID_TYPE_VENDOR_BEGIN       // 串口透传服务UUID类型：厂商自定义UUID
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

#define UART_TX_BUF_SIZE               256 // 串口发送缓存大小（字节数）
#define UART_RX_BUF_SIZE               256 // 串口接收缓存大小（字节数）

// 用于stack dump的错误代码，可以用于栈回退时确定堆栈位置
#define DEAD_BEEF 0xDEADBEEF

BLE_UARTS_DEF(m_uarts, NRF_SDH_BLE_TOTAL_LINK_COUNT); // 定义名称为m_uarts的串口透传服务实例
NRF_BLE_GATT_DEF(m_gatt);                             // 定义名称为m_gatt的GATT模块实例
NRF_BLE_QWR_DEF(m_qwr);                               // 定义一个名称为m_qwr的排队写入实例
BLE_ADVERTISING_DEF(m_advertising);                   // 定义名称为m_advertising的广播模块实例

// 该变量用于保存连接句柄，初始值设置为无连接
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
// 发送的最大数据长度
static uint16_t m_ble_uarts_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;
static bool uart_enabled                 = false;
// 定义串口透传服务UUID列表
static ble_uuid_t m_adv_uuids[] =
    {
        {BLE_UUID_UARTS_SERVICE, UARTS_SERVICE_UUID_TYPE}};

// GAP参数初始化，该函数配置需要的GAP参数，包括设备名称，外观特征、首选连接参数
static void gap_params_init(void)
{
    ret_code_t err_code;
    // 定义连接参数结构体变量
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;
    // 设置GAP的安全模式
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    // 设置GAP设备名称，使用英文设备名称
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));

    // 检查函数返回的错误代码
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

// GATT事件处理函数，该函数中处理MTU交换事件
void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt)
{
    // 如果是MTU交换事件
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)) {
        // 设置串口透传服务的有效数据长度（MTU-opcode-handle）
        m_ble_uarts_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_uarts_max_data_len, m_ble_uarts_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}

// 初始化GATT程序模块
static void gatt_init(void)
{
    // 初始化GATT程序模块
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
    // 设置ATT MTU的大小,这里设置的值为247
    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}

// 排队写入事件处理函数，用于处理排队写入模块的错误
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    // 检查错误代码
    APP_ERROR_HANDLER(nrf_error);
}

// 串口事件回调函数，串口初始化时注册，该函数中判断事件类型并进行处理
// 当接收的数据长度达到设定的最大值或者接收到换行符后，则认为一包数据接收完成，之后将接收的数据发送给主机
void uart_event_handle(app_uart_evt_t *p_event)
{
    static uint8_t data_array[BLE_UARTS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t err_code;
    // 判断事件类型
    switch (p_event->evt_type) {
        case APP_UART_DATA_READY: // 串口接收事件
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;
            // 接收串口数据，当接收的数据长度达到m_ble_uarts_max_data_len或者接收到换行符后认为一包数据接收完成
            if ((data_array[index - 1] == '\n') ||
                (data_array[index - 1] == '\r') ||
                (index >= m_ble_uarts_max_data_len)) {
                if (index > 1) {
                    NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                    NRF_LOG_HEXDUMP_DEBUG(data_array, index);
                    // 串口接收的数据使用notify发送给BLE主机
                    do {
                        uint16_t length = (uint16_t)index;
                        err_code        = ble_uarts_data_send(&m_uarts, data_array, &length, m_conn_handle);
                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
                            (err_code != NRF_ERROR_RESOURCES) &&
                            (err_code != NRF_ERROR_NOT_FOUND)) {
                            APP_ERROR_CHECK(err_code);
                        }
                    } while (err_code == NRF_ERROR_RESOURCES);
                }

                index = 0;
            }
            break;
        // 通讯错误事件，进入错误处理
        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;
        // FIFO错误事件，进入错误处理
        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}

// 串口配置
void uart_config(void)
{
    uint32_t err_code;

    // 定义串口通讯参数配置结构体并初始化
    const app_uart_comm_params_t comm_params =
        {
            RX_PIN_NUMBER,                  // 定义uart接收引脚
            TX_PIN_NUMBER,                  // 定义uart发送引脚
            RTS_PIN_NUMBER,                 // 定义uart RTS引脚，流控关闭后虽然定义了RTS和CTS引脚，但是驱动程序会忽略，不会配置这两个引脚，两个引脚仍可作为IO使用
            CTS_PIN_NUMBER,                 // 定义uart CTS引脚
            APP_UART_FLOW_CONTROL_DISABLED, // 关闭uart硬件流控
            false,                          // 禁止奇偶检验
            NRF_UART_BAUDRATE_115200        // uart波特率设置为115200bps
        };
    // 初始化串口，注册串口事件回调函数
    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);

    APP_ERROR_CHECK(err_code);
}

// 串口透传事件回调函数，串口透出服务初始化时注册
static void uarts_data_handler(ble_uarts_evt_t *p_evt)
{
    // 通知使能后才初始化串口
    if (p_evt->type == BLE_NUS_EVT_COMM_STARTED) {
        uart_reconfig();
    }
    // 通知关闭后，关闭串口
    else if (p_evt->type == BLE_NUS_EVT_COMM_STOPPED) {
        uart_reconfig();
    }
    // 判断事件类型:接收到新数据事件
    if ((p_evt->type == BLE_UARTS_EVT_RX_DATA) && (uart_enabled == true)) {
        uint32_t err_code;
        // 串口打印出接收的数据
        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++) {
            do {
                err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
                if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY)) {
                    NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
                    APP_ERROR_CHECK(err_code);
                }
            } while (err_code == NRF_ERROR_BUSY);
        }
        if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r') {
            while (app_uart_put('\n') == NRF_ERROR_BUSY)
                ;
        }
    }
    // 判断事件类型:发送就绪事件，该事件在后面的试验会用到，当前我们在该事件中翻转指示灯D4的状态，指示该事件的产生
    if (p_evt->type == BLE_UARTS_EVT_TX_RDY) {
        nrf_gpio_pin_toggle(LED_4);
    }
}
// 关机准备处理程序。在关闭过程中，将以1秒的间隔调用此函数，直到函数返回true。当函数返回true时，表示应用程序已准备好复位为DFU模式
static bool app_shutdown_handler(nrf_pwr_mgmt_evt_t event)
{
    switch (event) {
        case NRF_PWR_MGMT_EVT_PREPARE_DFU:
            NRF_LOG_INFO("Power management wants to reset to DFU mode.");
            // YOUR_JOB: Get ready to reset into DFU mode
            //
            // If you aren't finished with any ongoing tasks, return "false" to
            // signal to the system that reset is impossible at this stage.
            //
            // Here is an example using a variable to delay resetting the device.
            //
            // if (!m_ready_for_reset)
            // {
            //      return false;
            // }
            // else
            //{
            //
            //    // Device ready to enter
            //    uint32_t err_code;
            //    err_code = sd_softdevice_disable();
            //    APP_ERROR_CHECK(err_code);
            //    err_code = app_timer_stop_all();
            //    APP_ERROR_CHECK(err_code);
            //}
            break;

        default:
            // YOUR_JOB: Implement any of the other events available from the power management module:
            //      -NRF_PWR_MGMT_EVT_PREPARE_SYSOFF
            //      -NRF_PWR_MGMT_EVT_PREPARE_WAKEUP
            //      -NRF_PWR_MGMT_EVT_PREPARE_RESET
            return true;
    }

    NRF_LOG_INFO("Power management allowed to reset to DFU mode.");
    return true;
}

// 注册优先级为0的应用程序关闭处理程序
NRF_PWR_MGMT_HANDLER_REGISTER(app_shutdown_handler, 0);

// SoftDevice状态监视者事件处理函数
static void buttonless_dfu_sdh_state_observer(nrf_sdh_state_evt_t state, void *p_context)
{
    if (state == NRF_SDH_EVT_STATE_DISABLED) {
        // 表明Softdevice在复位之前已经禁用，告之bootloader启动时应跳过CRC
        nrf_power_gpregret2_set(BOOTLOADER_DFU_SKIP_CRC);

        // 进入system off模式
        nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
    }
}

// 注册SoftDevice状态监视者，用于SoftDevice状态改变或者即将改变时接收SoftDevice事件
NRF_SDH_STATE_OBSERVER(m_buttonless_dfu_state_obs, 0) =
    {
        .handler = buttonless_dfu_sdh_state_observer,
};
// 获取广播模式、间隔和超时时间
static void advertising_config_get(ble_adv_modes_config_t *p_config)
{
    memset(p_config, 0, sizeof(ble_adv_modes_config_t));

    p_config->ble_adv_fast_enabled  = true;
    p_config->ble_adv_fast_interval = APP_ADV_INTERVAL;
    p_config->ble_adv_fast_timeout  = APP_ADV_DURATION;
}

// 断开当前连接，设备准备进入bootloader之前，需要先断开连接
static void disconnect(uint16_t conn_handle, void *p_context)
{
    UNUSED_PARAMETER(p_context);
    // 断开当前连接
    ret_code_t err_code = sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("Failed to disconnect connection. Connection handle: %d Error: %d", conn_handle, err_code);
    } else {
        NRF_LOG_DEBUG("Disconnected connection handle %d", conn_handle);
    }
}
// DFU事件处理函数。如果需要在DFU事件中执行操作，可以在相应的事件里面加入处理代码
static void ble_dfu_evt_handler(ble_dfu_buttonless_evt_type_t event)
{
    switch (event) {
        case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE: // 该事件指示设备正在准备进入bootloader
        {
            NRF_LOG_INFO("Device is preparing to enter bootloader mode.");

            // 阻止设备在断开连接时广播
            ble_adv_modes_config_t config;
            advertising_config_get(&config);
            // 连接断开后设备不自动进行广播
            config.ble_adv_on_disconnect_disabled = true;
            // 修改广播配置
            ble_advertising_modes_config_set(&m_advertising, &config);

            // 断开当前已经连接的所有其他绑定设备。在设备固件更新成功（或中止）后，需要在启动时接收服务更改指示
            uint32_t conn_count = ble_conn_state_for_each_connected(disconnect, NULL);
            NRF_LOG_INFO("Disconnected %d links.", conn_count);
            break;
        }

        case BLE_DFU_EVT_BOOTLOADER_ENTER: // 该事件指示函数返回后设备即进入bootloader
            // 如果应用程序有数据需要保存到Flash，通过app_shutdown_handler返回flase以延迟复位，从而保证数据正确写入到Flash
            NRF_LOG_INFO("Device will enter bootloader mode.");
            break;

        case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED: // 该事件指示进入bootloader失败
            NRF_LOG_ERROR("Request to enter bootloader mode failed asynchroneously.");
            // 进入bootloader失败，应用程序需要采取纠正措施来处理问题，如使用APP_ERROR_CHECK复位设备
            break;

        case BLE_DFU_EVT_RESPONSE_SEND_ERROR: // 该事件指示发送响应失败
            NRF_LOG_ERROR("Request to send a response to client failed.");
            // 发送响应失败，应用程序需要采取纠正措施来处理问题，如使用APP_ERROR_CHECK复位设备
            APP_ERROR_CHECK(false);
            break;

        default:
            NRF_LOG_ERROR("Unknown event from ble_dfu_buttonless.");
            break;
    }
}
// 服务初始化，包含初始化排队写入模块和初始化应用程序使用的服务
static void services_init(void)
{
    ret_code_t err_code;
    nrf_ble_qwr_init_t qwr_init         = {0}; // 定义排队写入初始化结构体变量
    ble_dfu_buttonless_init_t dfus_init = {0};

    // 排队写入事件处理函数
    qwr_init.error_handler = nrf_qwr_error_handler;
    // 初始化排队写入模块
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    // 检查函数返回值
    APP_ERROR_CHECK(err_code);

    /*------------------以下代码初始化日志透传服务-------------*/

    /*------------------初始化串口透传服务-END-----------------*/

    // 初始化DFU服务
    dfus_init.evt_handler = ble_dfu_evt_handler;

    err_code = ble_dfu_buttonless_init(&dfus_init);
    APP_ERROR_CHECK(err_code);
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
    init.advdata.include_appearance = false;
    // Flag:一般可发现模式，不支持BR/EDR
    init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    // UUID放到扫描响应里面
    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

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
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            // 打印提示信息
            NRF_LOG_INFO("Disconnected.");
            break;

        // 连接事件
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected.");

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
        // 系统属性访问正在等待中
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // 系统属性没有存储，更新系统属性
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;
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

// 初始化APP定时器模块
static void timers_init(void)
{
    // 初始化APP定时器模块
    ret_code_t err_code = app_timer_init();
    // 检查返回值
    APP_ERROR_CHECK(err_code);

    // 加入创建用户定时任务的代码，创建用户定时任务。
}

// 启动广播，该函数所用的模式必须和广播初始化中设置的广播模式一样
static void advertising_start(void)
{
    // 使用广播初始化中设置的广播模式启动广播
    ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
}

// 主函数
int ble_dfu_init(void)
{
    ret_code_t err_code;

    // 使能中断之前，初始化异步SVCI接口到Bootloader
    err_code = ble_dfu_buttonless_async_svci_init();
    APP_ERROR_CHECK(err_code);

    // 初始化APP定时器
    timers_init();

    // 初始化协议栈
    ble_stack_init();
    // 配置GAP参数
    gap_params_init();
    // 初始化GATT
    gatt_init();
    // 初始化服务
    services_init();
    // 初始化广播
    advertising_init();
    // 连接参数协商初始化
    conn_params_init();

    NRF_LOG_INFO("BLE Template example started.");
    // 启动广播
    advertising_start();
}
INIT_APP_EXPORT(ble_dfu_init);

/**
 * @file ble_common.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-19
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "ble_common.h"

// 用于stack dump的错误代码，可以用于栈回退时确定堆栈位置
#define DEAD_BEEF 0xDEADBEEF

/** @brief 全局变量 */
uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
uint16_t max_data_len  = BLE_GATT_ATT_MTU_DEFAULT - 3;

/** @brief 私有变量 */
/** @brief 该变量用于保存连接句柄，初始值设置为无连接 */
static const ble_serv_init_fn *serv_table[] = BLE_SERVICE_INIT_TABLE;
static const ble_uuid_t *uuids[]            = BLE_UUID_SERVICE_TABLE;
static const size_t uuids_len               = sizeof(uuids) / sizeof(uuids[0]);

/** @brief 私有函数 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context);
NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL); // 注册BLE事件回调函
/**
 * @brief GAP参数初始化，该函数配置需要的GAP参数，包括设备名称，外观特征、首选连接参数
 */
static void
gap_params_init(void)
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

/**
 * @brief GATT事件处理函数，在该函数中处理MTU交换事件
 *
 * @param p_gatt 通用属性配置控制句柄
 * @param p_evt 属性配置事件
 */
void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt)
{
    // 如果是MTU交换事件
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)) {
        // 设置串口透传服务的有效数据长度（MTU-opcode-handle）
        max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", max_data_len, max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}

/**
 * @brief 初始化GATT程序模块
 */
static void gatt_init(void)
{
    // 初始化GATT程序模块
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
    // 设置ATT MTU的大小,这里设置的值为23
    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief 排队写入事件处理函数，用于处理排队写入模块的错误
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    // 检查错误代码
    APP_ERROR_HANDLER(nrf_error);
}

/** @brief 服务初始化，包含初始化排队写入模块和初始化应用程序使用的服务 */
static void services_init(void)
{
    nrf_ble_qwr_init_t qwr_init = {0};
    qwr_init.error_handler      = nrf_qwr_error_handler;
    APP_ERROR_CHECK(nrf_ble_qwr_init(&m_qwr, &qwr_init));

    ble_serv_init_fn func = 0;
    for (size_t i = 0; i < sizeof(serv_table) / sizeof(serv_table[0]); i++) {
        func = *serv_table[i];
        func();
    }
}

/** @brief 连接参数协商模块事件处理函数 */
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

/** @brief 连接参数协商模块错误处理事件，参数nrf_error包含了错误代码，通过nrf_error可以分析错误信息 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    // 检查错误代码
    APP_ERROR_HANDLER(nrf_error);
}

/** @brief 连接参数协商模块初始化 */
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

/** @brief 广播事件处理函数*/
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;
    // 判断广播事件类型
    switch (ble_adv_evt) {
            // 快速广播启动事件：快速广播启动后会产生该事件
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("Fast advertising.");
            APP_ERROR_CHECK(err_code);
            break;
        // 广播IDLE事件：广播超时后会产生该事件
        case BLE_ADV_EVT_IDLE:
            break;

        default:
            break;
    }
}

/** @brief 广播初始化 */
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
    init.srdata.uuids_complete.uuid_cnt = uuids_len;
    init.srdata.uuids_complete.p_uuids  = (ble_uuid_t *)uuids[0];

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

/** @brief BLE事件处理函数 */
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

/** @brief 初始化BLE协议栈 */
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
}

/**
 * @brief 初始化软定时器
 *
 */
static void timers_init(void)
{
    // 初始化APP定时器模块
    ret_code_t err_code = app_timer_init();
    // 检查返回值
    APP_ERROR_CHECK(err_code);
    // 加入创建用户定时任务的代码，创建用户定时任务。
}

/**
 * @brief  启动广播，该函数所用的模式必须和广播初始化中设置的广播模式一样
 */
static void advertising_start(void)
{
    // 使用广播初始化中设置的广播模式启动广播
    ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    // 检查函数返回的错误代码
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief 初始化ble服务
 *
 * @return int
 */
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

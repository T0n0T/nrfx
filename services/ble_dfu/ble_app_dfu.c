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

#include "ble_common.h"

// DFU需要引用的头文件
#include "nrfx_gpiote.h"
#include "nrf_dfu_ble_svci_bond_sharing.h"
#include "nrf_svci_async_function.h"
#include "nrf_svci_async_handler.h"
#include "nrf_power.h"
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"

/**
 * @brief dfu func
 */
// 关机准备处理程序。在关闭过程中，将以1秒的间隔调用此函数，直到函数返回true。当函数返回true时，表示应用程序已准备好复位为DFU模式
static bool app_shutdown_handler(nrf_pwr_mgmt_evt_t event)
{
    switch (event) {
        case NRF_PWR_MGMT_EVT_PREPARE_DFU:
            NRF_LOG_INFO("Power management wants to reset to DFU mode.");
            NRF_LOG_INFO("Power management allowed to reset to DFU mode.");
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
        case NRF_PWR_MGMT_EVT_PREPARE_SYSOFF:
            break;
        case NRF_PWR_MGMT_EVT_PREPARE_WAKEUP:
            nrf_gpio_cfg_input(14, NRF_GPIO_PIN_PULLUP); // 将要唤醒的脚配置为输入
            /* 设置为上升沿检出，这个一定不要配置错了 */
            nrf_gpio_pin_sense_t sense = NRF_GPIO_PIN_SENSE_HIGH;
            nrf_gpio_cfg_sense_set(14, sense);
            break;
        case NRF_PWR_MGMT_EVT_PREPARE_RESET:
            break;
        default:
            // YOUR_JOB: Implement any of the other events available from the power management module:
            //      -NRF_PWR_MGMT_EVT_PREPARE_SYSOFF
            //      -NRF_PWR_MGMT_EVT_PREPARE_WAKEUP
            //      -NRF_PWR_MGMT_EVT_PREPARE_RESET
            return true;
    }

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
        .handler = buttonless_dfu_sdh_state_observer};

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

static void service_init(void)
{
    // ble_dfu_buttonless_init_t dfus_init = {0};
    // dfus_init.evt_handler               = ble_dfu_evt_handler;
    // APP_ERROR_CHECK(ble_dfu_buttonless_init(&dfus_init));
}
ble_serv_init_fn ble_app_dfu_init = service_init;

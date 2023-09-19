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
// ���õ�C��ͷ�ļ�
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
// Log��Ҫ���õ�ͷ�ļ�
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ble_common.h"

// DFU��Ҫ���õ�ͷ�ļ�
#include "nrf_dfu_ble_svci_bond_sharing.h"
#include "nrf_svci_async_function.h"
#include "nrf_svci_async_handler.h"
#include "nrf_power.h"
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"

/**
 * @brief dfu func
 */
// �ػ�׼����������ڹرչ����У�����1��ļ�����ô˺�����ֱ����������true������������trueʱ����ʾӦ�ó�����׼���ø�λΪDFUģʽ
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

// ע�����ȼ�Ϊ0��Ӧ�ó���رմ������
NRF_PWR_MGMT_HANDLER_REGISTER(app_shutdown_handler, 0);

// SoftDevice״̬�������¼�������
static void buttonless_dfu_sdh_state_observer(nrf_sdh_state_evt_t state, void *p_context)
{
    if (state == NRF_SDH_EVT_STATE_DISABLED) {
        // ����Softdevice�ڸ�λ֮ǰ�Ѿ����ã���֮bootloader����ʱӦ����CRC
        nrf_power_gpregret2_set(BOOTLOADER_DFU_SKIP_CRC);

        // ����system offģʽ
        nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
    }
}

// ע��SoftDevice״̬�����ߣ�����SoftDevice״̬�ı���߼����ı�ʱ����SoftDevice�¼�
NRF_SDH_STATE_OBSERVER(m_buttonless_dfu_state_obs, 0) =
    {
        .handler = buttonless_dfu_sdh_state_observer};

// ��ȡ�㲥ģʽ������ͳ�ʱʱ��
static void advertising_config_get(ble_adv_modes_config_t *p_config)
{
    memset(p_config, 0, sizeof(ble_adv_modes_config_t));

    p_config->ble_adv_fast_enabled  = true;
    p_config->ble_adv_fast_interval = APP_ADV_INTERVAL;
    p_config->ble_adv_fast_timeout  = APP_ADV_DURATION;
}

// �Ͽ���ǰ���ӣ��豸׼������bootloader֮ǰ����Ҫ�ȶϿ�����
static void disconnect(uint16_t conn_handle, void *p_context)
{
    UNUSED_PARAMETER(p_context);
    // �Ͽ���ǰ����
    ret_code_t err_code = sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("Failed to disconnect connection. Connection handle: %d Error: %d", conn_handle, err_code);
    } else {
        NRF_LOG_DEBUG("Disconnected connection handle %d", conn_handle);
    }
}

// DFU�¼��������������Ҫ��DFU�¼���ִ�в�������������Ӧ���¼�������봦�����
static void ble_dfu_evt_handler(ble_dfu_buttonless_evt_type_t event)
{
    switch (event) {
        case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE: // ���¼�ָʾ�豸����׼������bootloader
        {
            NRF_LOG_INFO("Device is preparing to enter bootloader mode.");

            // ��ֹ�豸�ڶϿ�����ʱ�㲥
            ble_adv_modes_config_t config;
            advertising_config_get(&config);
            // ���ӶϿ����豸���Զ����й㲥
            config.ble_adv_on_disconnect_disabled = true;
            // �޸Ĺ㲥����
            ble_advertising_modes_config_set(&m_advertising, &config);

            // �Ͽ���ǰ�Ѿ����ӵ������������豸�����豸�̼����³ɹ�������ֹ������Ҫ������ʱ���շ������ָʾ
            uint32_t conn_count = ble_conn_state_for_each_connected(disconnect, NULL);
            NRF_LOG_INFO("Disconnected %d links.", conn_count);
            break;
        }

        case BLE_DFU_EVT_BOOTLOADER_ENTER: // ���¼�ָʾ�������غ��豸������bootloader
            // ���Ӧ�ó�����������Ҫ���浽Flash��ͨ��app_shutdown_handler����flase���ӳٸ�λ���Ӷ���֤������ȷд�뵽Flash
            NRF_LOG_INFO("Device will enter bootloader mode.");
            break;

        case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED: // ���¼�ָʾ����bootloaderʧ��
            NRF_LOG_ERROR("Request to enter bootloader mode failed asynchroneously.");
            // ����bootloaderʧ�ܣ�Ӧ�ó�����Ҫ��ȡ������ʩ���������⣬��ʹ��APP_ERROR_CHECK��λ�豸
            break;

        case BLE_DFU_EVT_RESPONSE_SEND_ERROR: // ���¼�ָʾ������Ӧʧ��
            NRF_LOG_ERROR("Request to send a response to client failed.");
            // ������Ӧʧ�ܣ�Ӧ�ó�����Ҫ��ȡ������ʩ���������⣬��ʹ��APP_ERROR_CHECK��λ�豸
            APP_ERROR_CHECK(false);
            break;

        default:
            NRF_LOG_ERROR("Unknown event from ble_dfu_buttonless.");
            break;
    }
}

static void services_init(void)
{
    ble_dfu_buttonless_init_t dfus_init = {0};
    dfus_init.evt_handler               = ble_dfu_evt_handler;
    APP_ERROR_CHECK(ble_dfu_buttonless_init(&dfus_init));
}

struct ble_service ble_app_dfu = {
    .ble_uuid.uuid      = 0,
    .ble_uuid.type      = 0,
    .ble_serv_init_func = services_init,
}
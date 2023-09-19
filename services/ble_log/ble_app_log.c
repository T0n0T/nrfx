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
#if NRF_MODULE_ENABLED(BLE_LOG)
#include "ble_app_log.h"

#include <stdlib.h>
#include <string.h>
#include "app_error.h"
#include "ble_gatts.h"
#include "ble_srv_common.h"
#include "nrf_log.h"

#define UART_TX_BUF_SIZE        256                        // ���ڷ��ͻ����С���ֽ�����
#define UART_RX_BUF_SIZE        256                        // ���ڽ��ջ����С���ֽ�����
#define UARTS_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN // ����͸������UUID���ͣ������Զ���UUID

BLE_UARTS_DEF(m_uarts, NRF_SDH_BLE_TOTAL_LINK_COUNT); // ��������Ϊm_uarts�Ĵ���͸������ʵ��

// ���͵�������ݳ���
static uint16_t m_ble_uarts_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;
static bool uart_enabled                 = false;
// ���崮��͸������UUID�б�
static ble_uuid_t m_adv_uuids[] =
    {
        {BLE_UUID_UARTS_SERVICE, UARTS_SERVICE_UUID_TYPE}};

// �����¼��ص����������ڳ�ʼ��ʱע�ᣬ�ú������ж��¼����Ͳ����д���
// �����յ����ݳ��ȴﵽ�趨�����ֵ���߽��յ����з�������Ϊһ�����ݽ�����ɣ�֮�󽫽��յ����ݷ��͸�����
void uart_event_handle(app_uart_evt_t *p_event)
{
    static uint8_t data_array[BLE_UARTS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t err_code;
    // �ж��¼�����
    switch (p_event->evt_type) {
        case APP_UART_DATA_READY: // ���ڽ����¼�
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;
            // ���մ������ݣ������յ����ݳ��ȴﵽm_ble_uarts_max_data_len���߽��յ����з�����Ϊһ�����ݽ������
            if ((data_array[index - 1] == '\n') ||
                (data_array[index - 1] == '\r') ||
                (index >= m_ble_uarts_max_data_len)) {
                if (index > 1) {
                    NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                    NRF_LOG_HEXDUMP_DEBUG(data_array, index);
                    // ���ڽ��յ�����ʹ��notify���͸�BLE����
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
        // ͨѶ�����¼������������
        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;
        // FIFO�����¼������������
        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}

// ��������
void uart_config(void)
{
    uint32_t err_code;
    APP_ERROR_CHECK(err_code);
}

// ����͸���¼��ص�����������͸�������ʼ��ʱע��
static void uarts_data_handler(ble_uarts_evt_t *p_evt)
{
    // ֪ͨʹ�ܺ�ų�ʼ������
    if (p_evt->type == BLE_NUS_EVT_COMM_STARTED) {
        uart_reconfig();
    }
    // ֪ͨ�رպ󣬹رմ���
    else if (p_evt->type == BLE_NUS_EVT_COMM_STOPPED) {
        uart_reconfig();
    }
    // �ж��¼�����:���յ��������¼�
    if ((p_evt->type == BLE_UARTS_EVT_RX_DATA) && (uart_enabled == true)) {
        uint32_t err_code;
        // ���ڴ�ӡ�����յ�����
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
    // �ж��¼�����:���;����¼������¼��ں����������õ�����ǰ�����ڸ��¼��з�תָʾ��D4��״̬��ָʾ���¼��Ĳ���
    if (p_evt->type == BLE_UARTS_EVT_TX_RDY) {
        nrf_gpio_pin_toggle(LED_4);
    }
}

#define BLE_UARTS_MAX_RX_CHAR_LEN BLE_UARTS_MAX_DATA_LEN // RX������󳤶ȣ��ֽ�����
#define BLE_UARTS_MAX_TX_CHAR_LEN BLE_UARTS_MAX_DATA_LEN // TX������󳤶ȣ��ֽ�����

static void on_connect(ble_uarts_t *p_uarts, ble_evt_t const *p_ble_evt)
{
    ret_code_t err_code;
    ble_uarts_evt_t evt;
    ble_gatts_value_t gatts_val;
    uint8_t cccd_value[2];
    ble_uarts_client_context_t *p_client = NULL;

    err_code = blcm_link_ctx_get(p_uarts->p_link_ctx_storage,
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
                                      p_uarts->rx_handles.cccd_handle,
                                      &gatts_val);

    if ((err_code == NRF_SUCCESS) &&
        (p_uarts->data_handler != NULL) &&
        ble_srv_is_notification_enabled(gatts_val.p_value)) {
        if (p_client != NULL) {
            p_client->is_notification_enabled = true;
        }

        memset(&evt, 0, sizeof(ble_uarts_evt_t));
        evt.type        = BLE_NUS_EVT_COMM_STARTED;
        evt.p_uarts     = p_uarts;
        evt.conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        evt.p_link_ctx  = p_client;

        p_uarts->data_handler(&evt);
    }
}

// SoftDevice�ύ��"write"�¼�������
static void on_write(ble_uarts_t *p_uarts, ble_evt_t const *p_ble_evt)
{
    ret_code_t err_code;
    // ����һ������͸���¼��ṹ�����������ִ�лص�ʱ���ݲ���
    ble_uarts_evt_t evt;
    ble_uarts_client_context_t *p_client;
    // ����write�¼��ṹ��ָ�벢ָ��GATT�¼��е�wirite
    ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    err_code = blcm_link_ctx_get(p_uarts->p_link_ctx_storage,
                                 p_ble_evt->evt.gatts_evt.conn_handle,
                                 (void *)&p_client);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gatts_evt.conn_handle);
    }

    // ����evt
    memset(&evt, 0, sizeof(ble_uarts_evt_t));
    // ָ�򴮿�͸������ʵ��
    evt.p_uarts = p_uarts;
    // �������Ӿ��
    evt.conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
    evt.p_link_ctx  = p_client;
    // дCCCD�����ҳ��ȵ���2
    if ((p_evt_write->handle == p_uarts->tx_handles.cccd_handle) &&
        (p_evt_write->len == 2)) {
        if (p_client != NULL) {
            if (ble_srv_is_notification_enabled(p_evt_write->data)) {
                p_client->is_notification_enabled = true;
                evt.type                          = BLE_NUS_EVT_COMM_STARTED;
            } else {
                p_client->is_notification_enabled = false;
                evt.type                          = BLE_NUS_EVT_COMM_STOPPED;
            }

            if (p_uarts->data_handler != NULL) {
                p_uarts->data_handler(&evt);
            }
        }
    }
    // дRX����ֵ
    else if ((p_evt_write->handle == p_uarts->rx_handles.value_handle) &&
             (p_uarts->data_handler != NULL)) {
        // �����¼�����
        evt.type = BLE_UARTS_EVT_RX_DATA;
        // �������ݳ���
        evt.params.rx_data.p_data = p_evt_write->data;
        // �������ݳ���
        evt.params.rx_data.length = p_evt_write->len;
        // ִ�лص�
        p_uarts->data_handler(&evt);
    } else {
        // ���¼��ʹ���͸�������޹أ�����
    }
}

// SoftDevice�ύ��"BLE_GATTS_EVT_HVN_TX_COMPLETE"�¼�������
static void on_hvx_tx_complete(ble_uarts_t *p_uarts, ble_evt_t const *p_ble_evt)
{
    ret_code_t err_code;
    // ����һ������͸���¼��ṹ�����evt������ִ�лص�ʱ���ݲ���
    ble_uarts_evt_t evt;

    ble_uarts_client_context_t *p_client;

    err_code = blcm_link_ctx_get(p_uarts->p_link_ctx_storage,
                                 p_ble_evt->evt.gatts_evt.conn_handle,
                                 (void *)&p_client);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gatts_evt.conn_handle);
    }

    // ֪ͨʹ�ܵ�����²Ż�ִ�лص�
    if (p_client->is_notification_enabled) {
        // ����evt
        memset(&evt, 0, sizeof(ble_uarts_evt_t));
        // �����¼�����
        evt.type = BLE_UARTS_EVT_TX_RDY;
        // ָ�򴮿�͸������ʵ��
        evt.p_uarts = p_uarts;
        // �������Ӿ��
        evt.conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
        // ָ��link context
        evt.p_link_ctx = p_client;
        // ִ�лص�
        p_uarts->data_handler(&evt);
    }
}

// ����͸������BLE�¼������ߵ��¼��ص�����
void ble_uarts_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
    // �������Ƿ���Ч
    if ((p_context == NULL) || (p_ble_evt == NULL)) {
        return;
    }
    // ����һ������͸���ṹ��ָ�벢ָ�򴮿�͸���ṹ��
    ble_uarts_t *p_uarts = (ble_uarts_t *)p_context;
    // �ж��¼�����
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED: // ���ӽ����¼�
            on_connect(p_uarts, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE: // д�¼�
                                  // ����д�¼�
            on_write(p_uarts, p_ble_evt);
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE: // TX�����¼�
                                            // ����TX�����¼�
            on_hvx_tx_complete(p_uarts, p_ble_evt);
            break;

        default:
            break;
    }
}

// ��ʼ������͸������
uint32_t ble_uarts_init(ble_uarts_t *p_uarts, ble_uarts_init_t const *p_uarts_init)
{
    ret_code_t err_code;
    // ����16λUUID�ṹ�����
    ble_uuid_t ble_uuid;
    // ����128λUUID�ṹ�����������ʼ��Ϊ����͸������UUID����
    ble_uuid128_t nus_base_uuid = UARTS_BASE_UUID;
    // �������������ṹ�����
    ble_add_char_params_t add_char_params;
    // ���ָ���Ƿ�ΪNULL
    VERIFY_PARAM_NOT_NULL(p_uarts);
    VERIFY_PARAM_NOT_NULL(p_uarts_init);

    // ��������͸�������ʼ���ṹ���е��¼����
    p_uarts->data_handler = p_uarts_init->data_handler;

    // ���Զ���UUID������ӵ�SoftDevice
    err_code = sd_ble_uuid_vs_add(&nus_base_uuid, &p_uarts->uuid_type);
    VERIFY_SUCCESS(err_code);
    // UUID���ͺ���ֵ��ֵ��ble_uuid����
    ble_uuid.type = p_uarts->uuid_type;
    ble_uuid.uuid = BLE_UUID_UARTS_SERVICE;

    // ��Ӵ���͸������
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_uarts->service_handle);
    VERIFY_SUCCESS(err_code);
    /*---------------------���´������RX����--------------------*/
    // ���RX����
    // ���ò���֮ǰ������add_char_params
    memset(&add_char_params, 0, sizeof(add_char_params));
    // RX������UUID
    add_char_params.uuid = BLE_UUID_UARTS_RX_CHARACTERISTIC;
    // RX������UUID����
    add_char_params.uuid_type = p_uarts->uuid_type;
    // ����RX����ֵ����󳤶�
    add_char_params.max_len = BLE_UARTS_MAX_RX_CHAR_LEN;
    // ����RX����ֵ�ĳ�ʼ����
    add_char_params.init_len = sizeof(uint8_t);
    // ����RX������ֵ����Ϊ�ɱ䳤��
    add_char_params.is_var_len = true;
    // ����RX����������֧��д
    add_char_params.char_props.write = 1;
    // ����RX����������֧������Ӧд
    add_char_params.char_props.write_wo_resp = 1;
    // ���ö�/дRX����ֵ�İ�ȫ�����ް�ȫ��
    add_char_params.read_access  = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;
    // Ϊ����͸���������RX����
    err_code = characteristic_add(p_uarts->service_handle, &add_char_params, &p_uarts->rx_handles);
    // ��鷵�صĴ������
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    /*---------------------���RX����-END------------------------*/

    /*---------------------���´������TX����--------------------*/
    // ���TX����
    // ���ò���֮ǰ������add_char_params
    memset(&add_char_params, 0, sizeof(add_char_params));
    // TX������UUID
    add_char_params.uuid = BLE_UUID_UARTS_TX_CHARACTERISTIC;
    // TX������UUID����
    add_char_params.uuid_type = p_uarts->uuid_type;
    // ����TX����ֵ����󳤶�
    add_char_params.max_len = BLE_UARTS_MAX_TX_CHAR_LEN;
    // ����TX����ֵ�ĳ�ʼ����
    add_char_params.init_len = sizeof(uint8_t);
    // ����TX������ֵ����Ϊ�ɱ䳤��
    add_char_params.is_var_len = true;
    // ����TX���������ʣ�֧��֪ͨ
    add_char_params.char_props.notify = 1;
    // ���ö�/дRX����ֵ�İ�ȫ�����ް�ȫ��
    add_char_params.read_access       = SEC_OPEN;
    add_char_params.write_access      = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;
    // Ϊ����͸���������TX����
    return characteristic_add(p_uarts->service_handle, &add_char_params, &p_uarts->tx_handles);
    /*---------------------���TX����-END------------------------*/
}

uint32_t ble_uarts_data_send(ble_uarts_t *p_uarts,
                             uint8_t *p_data,
                             uint16_t *p_length,
                             uint16_t conn_handle)
{
    ret_code_t err_code;
    ble_gatts_hvx_params_t hvx_params;
    ble_uarts_client_context_t *p_client;
    // ��֤p_uartsû��ָ��NULL
    VERIFY_PARAM_NOT_NULL(p_uarts);

    err_code = blcm_link_ctx_get(p_uarts->p_link_ctx_storage, conn_handle, (void *)&p_client);
    VERIFY_SUCCESS(err_code);

    // ������Ӿ����Ч����ʾû�к������������ӣ�����NRF_ERROR_NOT_FOUND
    if ((conn_handle == BLE_CONN_HANDLE_INVALID) || (p_client == NULL)) {
        return NRF_ERROR_NOT_FOUND;
    }
    // ���֪ͨû��ʹ�ܣ�����״̬��Ч�Ĵ������
    if (!p_client->is_notification_enabled) {
        return NRF_ERROR_INVALID_STATE;
    }
    // ������ݳ��ȳ��ޣ����ز�����Ч�Ĵ������
    if (*p_length > BLE_UARTS_MAX_DATA_LEN) {
        return NRF_ERROR_INVALID_PARAM;
    }
    // ����֮ǰ������hvx_params
    memset(&hvx_params, 0, sizeof(hvx_params));
    // TX����ֵ���
    hvx_params.handle = p_uarts->tx_handles.value_handle;
    // ���͵�����
    hvx_params.p_data = p_data;
    // ���͵����ݳ���
    hvx_params.p_len = p_length;
    // ����Ϊ֪ͨ
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    // ����TX����ֵ֪ͨ
    return sd_ble_gatts_hvx(conn_handle, &hvx_params);
}

static void services_init(void)
{
    ble_uarts_init_t uarts_init = {0};
    uarts_init.data_handler     = uarts_data_handler;
    APP_ERROR_CHECK(ble_uarts_init(&m_uarts, &uarts_init));
}

ble_uuid_t ble_app_log = {
    .uuid = BLE_UUID_UARTS_SERVICE,
    .type = UARTS_SERVICE_UUID_TYPE,
}

#endif // NRF_MODULE_ENABLED(BLE_DIS)

/**
 * @file data_wr.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-11-01
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ringbuf.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "nrf_fstorage_sd.h"

#define NRF_LOG_MODULE_NAME config_wr
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define TX_BUF_SIZE 512 // 日志发送缓存大小（字节数）
#define RX_BUF_SIZE 512 // 接收缓存大小（字节数）

static TaskHandle_t         data_wr_task_handle = NULL;
static StreamBufferHandle_t data_wr_buf         = NULL;
static bool                 data_wr_enabled     = false;
static char                 tx_buf[TX_BUF_SIZE];

/* nrf library cfg */
BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT); /**< nus service instance. */
NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) = {
    .evt_handler = NULL,
    .start_addr  = 0x60000,
    .end_addr    = 0x6ffff,
};

/*------- fstorage handle---------*/
static void fstorage_evt_handler(nrf_fstorage_evt_t* p_evt)
{
    if (p_evt->result != NRF_SUCCESS) {
        NRF_LOG_ERROR("--> Event received: ERROR while executing an fstorage operation.");
        return;
    }

    switch (p_evt->id) {
        case NRF_FSTORAGE_EVT_WRITE_RESULT: {
            NRF_LOG_INFO("--> Event received: wrote %d bytes at address 0x%x.",
                         p_evt->len, p_evt->addr);
        } break;

        case NRF_FSTORAGE_EVT_ERASE_RESULT: {
            NRF_LOG_INFO("--> Event received: erased %d page from address 0x%x.",
                         p_evt->len, p_evt->addr);
        } break;

        default:
            break;
    }
}

void ble_nus_output(const char* str)
{
    if (data_wr_enabled) {
        xStreamBufferSend(data_wr_buf, (void*)str, strlen(str), 0);
    }
}

static void ble_tx_flush_process(void)
{
    // 判断日志使能
    if (data_wr_enabled) {
        uint32_t       err_code;
        static uint8_t data_array[TX_BUF_SIZE];
        uint16_t       data_len = 0;
        uint8_t*       index    = 0;
        // 获取日志数据
        data_len = xStreamBufferReceive(data_wr_buf, &data_array, TX_BUF_SIZE, portMAX_DELAY);
        index    = &data_array[0];
        while (data_len > 0) {
            if (data_len >= m_ble_nus_max_data_len) {
                NRF_LOG_INFO("Ready to send data len %d over BLE LOG", m_ble_nus_max_data_len);
                // 日志接收的数据使用notify发送给BLE主机
                do {
                    err_code = ble_nus_data_send(&m_nus, index, &m_ble_nus_max_data_len, m_conn_handle);
                    if ((err_code != NRF_ERROR_INVALID_STATE) &&
                        (err_code != NRF_ERROR_RESOURCES) &&
                        (err_code != NRF_ERROR_NOT_FOUND)) {
                        APP_ERROR_CHECK(err_code);
                    }
                } while (err_code == NRF_ERROR_RESOURCES);
                index += m_ble_nus_max_data_len;
                data_len -= m_ble_nus_max_data_len;
            } else {
                NRF_LOG_INFO("Ready to send data len %d over BLE LOG", data_len);
                // 日志接收的数据使用notify发送给BLE主机
                do {
                    err_code = ble_nus_data_send(&m_nus, index, &data_len, m_conn_handle);
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

void write_cfg(config_t* cfg)
{
    uint32_t err_code;
    err_code = nrf_fstorage_erase(&fstorage, fstorage.start_addr, 1, NULL);
    while (nrf_fstorage_is_busy(&fstorage)) {
        sd_app_evt_wait();
    }
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("earse flash fail!");
        ble_nus_output("earse flash fail!\r\n");
        return;
    }

    err_code = nrf_fstorage_write(&fstorage, fstorage.start_addr, cfg, sizeof(config_t), NULL);
    while (nrf_fstorage_is_busy(&fstorage)) {
        sd_app_evt_wait();
    }
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("write config fail!");
        ble_nus_output("flash write fail!\r\n");
        return;
    }

    NRF_LOG_INFO("write config success!");
    ble_nus_output("config parse success, restart to make config work\r\n");
}

int read_cfg(config_t* cfg)
{
    uint32_t err_code;
    err_code = nrf_fstorage_read(&fstorage, fstorage.start_addr, cfg, sizeof(config_t));
    while (nrf_fstorage_is_busy(&fstorage)) {
        sd_app_evt_wait();
    }
    if (err_code == NRF_SUCCESS && cfg->mqtt_cfg.host[0] != '\0' && cfg->mqtt_cfg.host[0] != 0xff) {
        NRF_LOG_INFO("read mqtt config success");
        return EOK;
    } else {
        NRF_LOG_WARNING("flash read fail, using default config");
        NRF_LOG_INFO("using default config!");
        return -ERROR;
    }
}

static void nus_data_handler(ble_nus_evt_t* p_evt)
{
    uint32_t err_code;
    uint16_t len = 0;
    switch (p_evt->type) {
        case BLE_NUS_EVT_RX_DATA:
            if (data_wr_enabled) {
                char* recv_data = malloc(p_evt->params.rx_data.length);
                memcpy(recv_data, p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
                if (strncmp(recv_data, "update:", 7) == 0) {
                    config_t tmp_cfg = {0};
                    err_code         = parse_cfg(&recv_data[8], &tmp_cfg);
                    if (err_code != EOK) {
                        char* wrong_str = "config parse fail\r\n";
                        ble_nus_output(wrong_str);
                    } else {
                        memcpy(&global_cfg, &tmp_cfg, sizeof(config_t));
                        write_cfg(&global_cfg);
                    }

                } else if (strncmp(recv_data, "check", 5) == 0) {
                    char* cfg_str = build_msg_cfg(&global_cfg);
                    ble_nus_output(cfg_str);
                    free(cfg_str);
                } else {
                    ble_nus_output("Wrong command\r\n");
                }
                free(recv_data);
            }

            break;
        case BLE_NUS_EVT_COMM_STARTED:
            vTaskResume(data_wr_task_handle);
            data_wr_enabled = true;
            char* wel_str   = "You can update or check config here with a json-format string\r\n\
                             -- using word [update:(json...)] to update setting\r\n\
                             -- uinsg word [check]  to check setting, than will recieve a json string build from current config\r\n";
            ble_nus_output(wel_str);
            break;

        case BLE_NUS_EVT_COMM_STOPPED:
            vTaskSuspend(data_wr_task_handle);
            xStreamBufferReset(data_wr_buf);
            data_wr_enabled = false;
            break;
        default:
            break;
    }
}

void cfg_wr_task(void* p)
{
    while (1) {
        ble_tx_flush_process();
        vTaskDelay(500);
    }
}

void nus_service_init(void)
{
    ble_nus_init_t nus_init;
    data_wr_buf = xStreamBufferCreate(512, 1);
    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));
    nus_init.data_handler = nus_data_handler;
    APP_ERROR_CHECK(ble_nus_init(&m_nus, &nus_init));
    BaseType_t xReturned = xTaskCreate(cfg_wr_task,
                                       "CFG_WR",
                                       1024,
                                       0,
                                       5,
                                       &data_wr_task_handle);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
    vTaskSuspend(data_wr_task_handle);
}
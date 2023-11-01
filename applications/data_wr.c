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
#include "nrf_fstorage_sd.h"

#define NRF_LOG_MODULE_NAME config_wr
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

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

/*------- nus handle---------*/
static void ble_write(char* str, uint16_t len)
{
    uint32_t err_code;
    do {
        err_code = ble_nus_data_send(&m_nus, (uint8_t*)str, &len, m_conn_handle);
        if ((err_code != NRF_ERROR_INVALID_STATE) &&
            (err_code != NRF_ERROR_RESOURCES) &&
            (err_code != NRF_ERROR_NOT_FOUND)) {
            APP_ERROR_CHECK(err_code);
        }
    } while (err_code == NRF_ERROR_RESOURCES);
}

void write_cfg(config_t* cfg)
{
    uint32_t err_code;
    err_code = nrf_fstorage_erase(&fstorage, fstorage.start_addr, 1, NULL);
    while (nrf_fstorage_is_busy(&fstorage)) {
        sd_app_evt_wait();
    }
    if (err_code != NRF_SUCCESS) {
        char* wrong_str = "earse flash fail!\r\n";
        NRF_LOG_ERROR("earse flash fail!");
        ble_write(wrong_str, strlen(wrong_str));
        return;
    }

    err_code = nrf_fstorage_write(&fstorage, fstorage.start_addr, cfg, sizeof(config_t), NULL);
    while (nrf_fstorage_is_busy(&fstorage)) {
        sd_app_evt_wait();
    }
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("write config fail!");
        char* wrong_str = "flash write fail!\r\n";
        ble_write(wrong_str, strlen(wrong_str));
        return;
    }

    char* succ_str = "config parse success, restart to make config work\r\n";
    NRF_LOG_INFO("write config success!");
    ble_write(succ_str, strlen(succ_str));
}

int read_cfg(config_t* cfg)
{
    uint32_t err_code;
    err_code = nrf_fstorage_read(&fstorage, fstorage.start_addr, cfg, sizeof(config_t));
    while (nrf_fstorage_is_busy(&fstorage)) {
        sd_app_evt_wait();
    }
    if (err_code == NRF_SUCCESS) {
        NRF_LOG_INFO("read mqtt config success");
        return EOK;
    } else {
        NRF_LOG_ERROR("flash read fail, using default config");
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
            char* recv_data = malloc(p_evt->params.rx_data.length);
            memcpy(recv_data, p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
            if (strncmp(recv_data, "update:", 7) == 0) {
                config_t tmp_cfg = {0};
                err_code         = parse_cfg(&recv_data[8], &tmp_cfg);
                if (err_code != EOK) {
                    char* wrong_str = "config parse fail\r\n";
                    ble_write(wrong_str, strlen(wrong_str));
                } else {
                    memcpy(&global_cfg, &tmp_cfg, sizeof(config_t));
                    write_cfg(&global_cfg);
                }

            } else if (strncmp(recv_data, "check:", 6) == 0) {
                char* cfg_str = build_msg_cfg(&global_cfg);
                ble_write(cfg_str, strlen(cfg_str));
            } else {
                char* wrong_str = "Wrong command\r\n";
                ble_write(wrong_str, strlen(wrong_str));
            }

            break;
        case BLE_NUS_EVT_COMM_STARTED:
            char* wel_str = "You can update or check config here with a json-format string\r\n\
                             -- using word [update:(json...)] to update setting\r\n\
                             -- uinsg word [check]  to check setting, than will recieve a json string build from current config\r\n";
            ble_write(wel_str, strlen(wel_str));
            break;
        default:
            break;
    }
}

void nus_service_init(void)
{
    ble_nus_init_t nus_init;
    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    APP_ERROR_CHECK(ble_nus_init(&m_nus, &nus_init));
}
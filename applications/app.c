/**
 * @file app.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-30
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "app.h"
#include "ccm3310.h"
#include "nrf_log.h"

static TaskHandle_t m_app_task;
static uint8_t      sm4_id = 0;
static uint8_t      sm4_flag;

config_t global_cfg = {
    .mqtt_cfg         = EC800M_MQTT_DEFAULT_CFG,
    .publish_interval = MQTT_PUBLISH_DEFAULT_INTERVAL,
    .sm4_flag         = SM4_ENABLED,
    .sm4_key          = SM4_DEFAULT_KEY,
};

static void read_cfg_from_flash(void)
{
    APP_ERROR_CHECK(nrf_fstorage_init(&fstorage, &nrf_fstorage_sd, NULL));
    while (nrf_fstorage_is_busy(&fstorage)) {
        sd_app_evt_wait();
    }
    config_t tmp_cfg = {0};
    if (read_cfg(&tmp_cfg) == EOK) {
        memcpy(&global_cfg, &tmp_cfg, sizeof(config_t));
    }
}

void publish_handle(void)
{
    if (mqtt_status != EC800M_MQTT_CONN) {
        return;
    }

    char* publish_data = build_msg_mqtt(mqtt_config.clientid, ec800m_gnss_get(), ecg_status, 1);

    // if (sm4_flag) {
    //     pdata      origin_mqtt = {strlen(publish_data), (uint8_t*)publish_data};
    //     ciphertext cipher_mqtt = ccm3310_sm4_encrypt(sm4_id, origin_mqtt);
    //     if (cipher_mqtt.len > 0) {

    //     } else {
    //         NRF_LOG_ERROR("publish msg sm4 encrypt fail.");
    //     }
    // } else {
    // }

    ec800m_mqtt_pub(mqtt_config.pubtopic, publish_data, strlen(publish_data));

    free(publish_data);
}

static int mqtt_init(void)
{
    ec800m_mqtt_t cfg = {0};

    read_cfg_from_flash();

    char* src = build_msg_cfg(&global_cfg);
    free(src);

    ec800m_mqtt_conf(&global_cfg.mqtt_cfg);
    while (ec800m.status != EC800M_IDLE) {
        vTaskDelay(200);
    }
    ec800m_mqtt_connect();
    while (mqtt_status != EC800M_MQTT_CONN) {
        vTaskDelay(200);
    }

    ec800m_mqtt_sub(global_cfg.mqtt_cfg.subtopic);
    if (global_cfg.sm4_flag) {
        sm4_id = ccm3310_sm4_setkey((uint8_t*)global_cfg.sm4_key);
        if (!sm4_id) {
            sm4_id = ccm3310_sm4_updatekey_ram(0x81, (uint8_t*)global_cfg.sm4_key);
            if (!sm4_id) {
                NRF_LOG_WARNING("mqtt app init success, but cannot get new keyid, work without sm4");
                sm4_flag = 0;
            } else {
                NRF_LOG_WARNING("mqtt app init success, updating and using keyid 0x81");
                sm4_flag = 1;
            }

        } else {
            NRF_LOG_INFO("mqtt app init success, keyid = 0x%02x", sm4_id);
            sm4_flag = 1;
        }
    }

    return 0;
}

void app_task(void* pvParameter)
{
    mqtt_init();
    // vTaskDelete(NULL);
    while (1) {
        publish_handle();
        NRF_LOG_INFO("mqtt app task loop");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_init(void)
{
    BaseType_t xReturned = xTaskCreate(app_task,
                                       "APP",
                                       512,
                                       0,
                                       configMAX_PRIORITIES - 3,
                                       &m_app_task);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("app task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
}

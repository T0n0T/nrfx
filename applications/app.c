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
SemaphoreHandle_t   m_app_sem;
uint8_t             sm4_flag;

#if !EC800M_MQTT_SOFT
mqtt_client_t* client      = NULL;
mqtt_message_t publish_msg = {0};
#endif

config_t global_cfg = {
    .mqtt_cfg         = EC800M_MQTT_DEFAULT_CFG,
    .publish_interval = MQTT_PUBLISH_DEFAULT_INTERVAL,
    .sm4_flag         = SM4_ENABLED,
    .sm4_key          = SM4_DEFAULT_KEY,
    .ble_mac          = BLE_ADDR,
};

void read_cfg_from_flash(void)
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
#if EC800M_MQTT_SOFT
    if (mqtt_status != EC800M_MQTT_CONN) {
        return;
    }
#endif
    char* publish_data = build_msg_mqtt(global_cfg.mqtt_cfg.clientid, ec800m_gnss_get(), 1, ecg_status);
    if (sm4_flag) {
        pdata      origin_mqtt = {strlen(publish_data), (uint8_t*)publish_data};
        ciphertext cipher_mqtt = ccm3310_sm4_encrypt(sm4_id, origin_mqtt);
        if (cipher_mqtt.len > 0) {
#if EC800M_MQTT_SOFT
            ec800m_mqtt_pub(global_cfg.mqtt_cfg.pubtopic, cipher_mqtt.data, cipher_mqtt.len);
#else
            memset(&publish_msg, 0, sizeof(publish_msg));
            publish_msg.qos        = QOS0;
            publish_msg.payloadlen = cipher_mqtt.len;
            publish_msg.payload    = cipher_mqtt.data;
            mqtt_publish(client, global_cfg.mqtt_cfg.pubtopic, &publish_msg);
#endif
        } else {
            NRF_LOG_ERROR("publish msg sm4 encrypt fail.");
        }
    } else {
#if EC800M_MQTT_SOFT
        ec800m_mqtt_pub(global_cfg.mqtt_cfg.pubtopic, publish_data, strlen(publish_data));
#else
        memset(&publish_msg, 0, sizeof(publish_msg));
        publish_msg.qos        = QOS0;
        publish_msg.payloadlen = strlen(publish_data);
        publish_msg.payload    = publish_data;
        mqtt_publish(client, global_cfg.mqtt_cfg.pubtopic, &publish_msg);
#endif
    }
    free(publish_data);
}

static int mqtt_init(void)
{
    // read_cfg_from_flash();
    while (ec800m.status != EC800M_IDLE) {
        NRF_LOG_DEBUG("wating for ec800m!")
        nrf_gpio_pin_write(LED2, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
#if EC800M_MQTT_SOFT
    ec800m_mqtt_conf(&global_cfg.mqtt_cfg);
    ec800m_mqtt_connect();
    while (mqtt_status != EC800M_MQTT_CONN) {
        vTaskDelay(200);
    }

    ec800m_mqtt_sub(global_cfg.mqtt_cfg.subtopic);
#else
    client = mqtt_lease();
    if (client == NULL) {
        NRF_LOG_ERROR("mqtt alloec fail");
        return -1;
    }
    mqtt_conf(client, &global_cfg);

    int err = mqtt_connect(client);
    if (err != MQTT_SUCCESS_ERROR) {
        NRF_LOG_ERROR("mqtt connect fail, err[%d]", err);
        return -1;
    }
    extern void sub_handle(void* client, message_data_t* msg);
    mqtt_subscribe(client, global_cfg.mqtt_cfg.subtopic, QOS1, sub_handle);
#endif
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
    if (mqtt_init() < 0) {
        while (1) {
            nrf_gpio_pin_write(LED2, 0);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
    nrf_gpio_pin_write(LED2, 0);
    nrf_gpio_pin_write(LED3, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
    nrf_gpio_pin_write(LED2, 1);
    nrf_gpio_pin_write(LED3, 1);
    while (1) {
        nrf_gpio_pin_write(LED3, 0);
        publish_handle();
        nrf_gpio_pin_write(LED3, 1);
        // NRF_LOG_INFO("mqtt app task loop");
        xSemaphoreTake(m_app_sem, pdMS_TO_TICKS(30000));
    }
}

void app_init(void)
{
    m_app_sem            = xSemaphoreCreateBinary();
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

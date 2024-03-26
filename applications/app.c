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
#include "bsp.h"
#include "ec800m.h"
#include "ccm3310.h"
#include "blood.h"
#include "nrf_log.h"

extern ec800m_t* ec800m;

static TaskHandle_t  m_app_task;
static TimerHandle_t m_app_timer;
static gps_info_t    m_gps_data;
static uint8_t       sm4_id;
SemaphoreHandle_t    m_app_sem;
uint8_t              sm4_flag;
uint8_t              working_flag;
uint8_t              battery_flag;

#if !EC800M_MQTT_SOFT
mqtt_client_t* client      = NULL;
mqtt_message_t publish_msg = {0};
#endif

config_t global_cfg = {
    .mqtt_cfg         = EC800M_MQTT_DEFAULT_CFG,
    .publish_interval = MQTT_PUBLISH_DEFAULT_INTERVAL,
    .sm4_flag         = 0,
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
    char* publish_data = build_msg_mqtt(global_cfg.mqtt_cfg.clientid, m_gps_data, 1, ecg_status);
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
#endif
    }
    mqtt_publish(client, global_cfg.mqtt_cfg.pubtopic, &publish_msg);
    free(publish_data);
}

static void mqtt_init(void)
{
    if (ec800m->status == EC800M_POWER_OFF) {
        NRF_LOG_DEBUG("power on");
        ec800m_power_on(ec800m);
        ec800m_gnss_open(ec800m);
    }
    if (ec800m->status != EC800M_POWER_OFF) {
        uint8_t retry = 3;
        m_gps_data    = NULL;
        while (retry--) {
            m_gps_data = ec800m_gnss_get(ec800m);
            if (m_gps_data == NULL || m_gps_data->AV == 'V') {
                vTaskDelay(pdMS_TO_TICKS(5000));
            } else {
                break;
            }
        }
#if EC800M_MQTT_SOFT
        ec800m_mqtt_conf(&global_cfg.mqtt_cfg);
        ec800m_mqtt_connect();
        while (mqtt_status != EC800M_MQTT_CONN) {
            vTaskDelay(200);
        }
        ec800m_mqtt_sub(global_cfg.mqtt_cfg.subtopic);
#else
        if (client->mqtt_client_state != CLIENT_STATE_CONNECTED) {
            NRF_LOG_DEBUG("begin connect");
            mqtt_connect(client);
        }
#endif
    }
}

static void mqtt_deinit(void)
{
    if (ec800m->status != EC800M_POWER_OFF) {
        if (client->mqtt_client_state == CLIENT_STATE_CONNECTED) {
            mqtt_disconnect(client);
            NRF_LOG_DEBUG("begin disconnect %d", xTaskGetTickCount());
        }
        ec800m_power_off(ec800m);
        NRF_LOG_DEBUG("power off");
    }
}

void app_task(void* pvParameter)
{
    client = mqtt_lease();
    if (client == NULL) {
        NRF_LOG_ERROR("mqtt alloc fail");
        return;
    }
    //expired timer for shutdown ec800m is set to 10s,publish interval should be largger than it.
    global_cfg.publish_interval = global_cfg.publish_interval > 20000 ?  global_cfg.publish_interval : 20000;
    mqtt_conf(client, &global_cfg);
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
        nrf_gpio_pin_write(GINT0, 1);
    }

    while (1) {
        working_flag = 1;
        NRF_LOG_DEBUG("looper begin %d", xTaskGetTickCount());
        xTimerStop(m_app_timer, 0);
        mqtt_init();
        if (client->mqtt_client_state == CLIENT_STATE_CONNECTED) {
            NRF_LOG_DEBUG("mqtt publish begin");
            blood_Loop();
            publish_handle();
            if (sm4_flag) {
                nrf_gpio_pin_write(GINT0, 1);
            }
            NRF_LOG_DEBUG("mqtt publish end");
        }

        xTimerStart(m_app_timer, 0);
        working_flag = 0;
        xSemaphoreTake(m_app_sem, pdMS_TO_TICKS(global_cfg.publish_interval));
    }
}

void app_timer_handler(void)
{
    mqtt_deinit();
}

void app_init(void)
{
    m_app_sem = xSemaphoreCreateBinary();

    m_app_timer          = xTimerCreate("app_tr", pdMS_TO_TICKS(10000), pdFALSE, (void*)0, (TimerCallbackFunction_t)app_timer_handler);
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

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
#include "time.h"
#include "cJSON.h"
#include "ec800m.h"
#include "blood.h"
#include "ccm3310.h"
#include "nrf_fstorage_sd.h"

#include "nrf_log.h"

#define FLASH_CFG_ADDR 0x60000

NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) =
    {
        .evt_handler = NULL,
        .start_addr  = FLASH_CFG_ADDR, // 开始地址
        .end_addr    = 0x7ffff,        // 结束地址
};

/*-------thread init---------*/
static TaskHandle_t m_app_task;

/*-------client init---------*/
uint8_t              mission_status = 0;
static ec800m_mqtt_t _cfg           = EC800M_MQTT_DEFAULT_CFG;
/*-------sm4 init---------*/
uint8_t              sm4_flag = 0;
static uint8_t       sm4_id   = 0;
const static uint8_t key[]    = {
       0x77, 0x7f, 0x23, 0xc6,
       0xfe, 0x7b, 0x48, 0x73,
       0xdd, 0x59, 0x5c, 0xff,
       0xf6, 0x5f, 0x58, 0xec};

char* build_msg_updata(char* device_id, gps_info_t info, int energyStatus, int correctlyWear);

void publish_handle(void)
{
    if (ec800m.status != EC800M_MQTT_CONN) {
        return;
    }

    char* publish_data = build_msg_updata(ec800m.mqtt.clientid, ec800m_gnss_get(), ecg_status, 1);

    // if (sm4_flag) {
    //     pdata      origin_mqtt = {strlen(publish_data), (uint8_t*)publish_data};
    //     ciphertext cipher_mqtt = ccm3310_sm4_encrypt(sm4_id, origin_mqtt);
    //     if (cipher_mqtt.len > 0) {

    //     } else {
    //         NRF_LOG_ERROR("publish msg sm4 encrypt fail.");
    //     }
    // } else {
    // }

    ec800m_mqtt_pub(ec800m.mqtt.pubtopic, publish_data, strlen(publish_data));

    free(publish_data);
}

static int mqtt_init(void)
{
    ec800m_mqtt_t cfg = {0};

    // if (nrf_fstorage_init(&fstorage, &nrf_fstorage_sd, NULL) == NRF_SUCCESS) {
    //     if (nrf_fstorage_read(&fstorage, FLASH_CFG_ADDR, &cfg, sizeof(struct ec800m_mqtt_t)) == NRF_SUCCESS) {
    //         NRF_LOG_INFO("read mqtt config success");
    //         ec800m_mqtt_conf(cfg);

    //     } else {
    //         NRF_LOG_ERROR("flash read fail, using default config");
    //         NRF_LOG_INFO("using default config!");
    //         ec800m_mqtt_conf(&_cfg);
    //     }
    // } else {
    //     NRF_LOG_ERROR("flash init fail");
    //     NRF_LOG_INFO("using default config!");
    //     ec800m_mqtt_conf(&_cfg);
    // }
    ec800m_mqtt_conf(&_cfg);
    ec800m_mqtt_connect();
    while (ec800m.status != EC800M_MQTT_CONN) {
        vTaskDelay(200);
    }

    ec800m_mqtt_sub(ec800m.mqtt.subtopic);

    // sm4_id = ccm3310_sm4_setkey((uint8_t*)key);
    // if (!sm4_id) {
    //     sm4_id = ccm3310_sm4_updatekey_ram(0x81, (uint8_t*)key);
    //     if (!sm4_id) {
    //         NRF_LOG_WARNING("mqtt app init success, but cannot get new keyid, work without sm4");
    //         sm4_flag = 0;
    //     } else {
    //         NRF_LOG_WARNING("mqtt app init success, updating and using keyid 0x81");
    //         sm4_flag = 1;
    //     }

    // } else {
    //     NRF_LOG_INFO("mqtt app init success, keyid = 0x%02x", sm4_id);
    //     sm4_flag = 1;
    // }
    return 0;
}

void app_task(void* pvParameter)
{
    mqtt_init();
    // vTaskDelete(NULL);
    while (1) {
        publish_handle();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_init(void)
{
    BaseType_t xReturned = xTaskCreate(app_task,
                                       "APP",
                                       512,
                                       0,
                                       5,
                                       &m_app_task);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("app task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
}

char* build_msg_updata(char* device_id, gps_info_t info, int energyStatus, int correctlyWear)
{
    cJSON *root, *body;
    char*  out;
    root = cJSON_CreateObject();

    struct tm tm_now = {0};
    tm_now.tm_year   = info->date.year - 1900;
    tm_now.tm_mon    = info->date.month - 1; /* .tm_min's range is [0-11] */
    tm_now.tm_mday   = info->date.day;
    tm_now.tm_hour   = info->date.hour;
    tm_now.tm_min    = info->date.minute;
    tm_now.tm_sec    = info->date.second;
    time_t now       = mktime(&tm_now);

    cJSON_AddNumberToObject(root, "timestamp", now);
    cJSON_AddStringToObject(root, "type", "SmartBraceletServiceUp");
    cJSON_AddItemToObject(root, "body", body = cJSON_CreateObject());
    cJSON_AddStringToObject(body, "deviceId", device_id);
    cJSON_AddNumberToObject(body, "routePointLon", info->coord.location.longitude.value);
    cJSON_AddNumberToObject(body, "routePointLat", info->coord.location.latitude.value);
    // cJSON_AddNumberToObject(body, "routePointEle", strtod(info->longitude, NULL));
    cJSON_AddNumberToObject(body, "routePointEle", 0);
    cJSON_AddNumberToObject(body, "energyStatus", energyStatus);
    cJSON_AddNumberToObject(body, "correctlyWear", correctlyWear);
    out = cJSON_Print(root);
    cJSON_Delete(root);
    return out;
}

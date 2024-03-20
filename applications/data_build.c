/**
 * @file data_build.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-11-01
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "app.h"
#include "cJSON.h"
#include "time.h"
#include "FreeRTOS.h"
#include "task.h"
#include "nrf_log.h"

static time_t old_ticks;
static time_t new_stamps;
static time_t _new_stamps;

char* build_msg_mqtt(char* device_id, gps_info_t info, int energyStatus, int correctlyWear)
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

    _new_stamps = mktime(&tm_now);

    new_stamps += ((time_t)xTaskGetTickCount() - old_ticks) * 1000 / configTICK_RATE_HZ;
    new_stamps = new_stamps > _new_stamps ? new_stamps : _new_stamps;
    old_ticks = (time_t)xTaskGetTickCount();

    cJSON_AddNumberToObject(root, "timestamp", new_stamps);
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

char* build_msg_cfg(config_t* config)
{
    cJSON *root, *mqtt;
    char*  out;
    char   sm4KeyhexStr[33] = {0};
    char   blehexStr[13]    = {0};
    for (int i = 0; i < 16; i++) {
        snprintf(sm4KeyhexStr + 2 * i, 3, "%02x", config->sm4_key[i]);
    }
    sm4KeyhexStr[32] = '\0';

    for (int i = 0; i < 6; i++) {
        snprintf(blehexStr + 2 * i, 3, "%02x", config->ble_mac[i]);
    }
    blehexStr[12] = '\0';

    root = cJSON_CreateObject();
    if (!root) {
        NRF_LOG_ERROR("no memory for json build");
    }

    cJSON_AddItemToObject(root, "mqtt", mqtt = cJSON_CreateObject());
    cJSON_AddStringToObject(mqtt, "host", config->mqtt_cfg.host);
    cJSON_AddStringToObject(mqtt, "port", config->mqtt_cfg.port);
    cJSON_AddNumberToObject(mqtt, "keepalive", config->mqtt_cfg.keepalive);
    cJSON_AddStringToObject(mqtt, "clientId", config->mqtt_cfg.clientid);
    cJSON_AddStringToObject(mqtt, "userName", config->mqtt_cfg.username);
    cJSON_AddStringToObject(mqtt, "passWord", config->mqtt_cfg.password);
    cJSON_AddStringToObject(mqtt, "subTopic", config->mqtt_cfg.subtopic);
    cJSON_AddStringToObject(mqtt, "pubTopic", config->mqtt_cfg.pubtopic);

    cJSON_AddNumberToObject(root, "publishInterval", config->publish_interval);
    cJSON_AddNumberToObject(root, "sm4Flag", config->sm4_flag);
    cJSON_AddStringToObject(root, "sm4Key", sm4KeyhexStr);
    cJSON_AddStringToObject(root, "bleMac", blehexStr);

    out = cJSON_Print(root);
    cJSON_Delete(root);
    return out;
}

int parse_cfg(char* json, config_t* config)
{
    cJSON * cfgjson = NULL, *mqttjson = NULL;
    uint8_t hex[16];
    char*   hexString;
    cfgjson = cJSON_Parse(json);

    if (!cfgjson) {
        NRF_LOG_ERROR("Error before: [%s]\n", cJSON_GetErrorPtr());
        return -EINVAL;
    }
    mqttjson = cJSON_GetObjectItem(cfgjson, "mqtt");
    strcpy(config->mqtt_cfg.host, cJSON_GetObjectItem(mqttjson, "host")->valuestring);
    strcpy(config->mqtt_cfg.port, cJSON_GetObjectItem(mqttjson, "port")->valuestring);
    config->mqtt_cfg.keepalive = cJSON_GetObjectItem(mqttjson, "keepalive")->valueint;
    strcpy(config->mqtt_cfg.clientid, cJSON_GetObjectItem(mqttjson, "clientId")->valuestring);
    strcpy(config->mqtt_cfg.username, cJSON_GetObjectItem(mqttjson, "userName")->valuestring);
    strcpy(config->mqtt_cfg.password, cJSON_GetObjectItem(mqttjson, "passWord")->valuestring);
    strcpy(config->mqtt_cfg.subtopic, cJSON_GetObjectItem(mqttjson, "subTopic")->valuestring);
    strcpy(config->mqtt_cfg.pubtopic, cJSON_GetObjectItem(mqttjson, "pubTopic")->valuestring);
    config->publish_interval = cJSON_GetObjectItem(cfgjson, "publishInterval")->valueint;
    config->sm4_flag         = cJSON_GetObjectItem(cfgjson, "sm4Flag")->valueint;
    hexString                = cJSON_GetObjectItem(cfgjson, "sm4Key")->valuestring;
    for (int i = 0; i < 32; i += 2) {
        char byteString[3] = {hexString[i], hexString[i + 1], '\0'};
        hex[i / 2]         = (uint8_t)strtol(byteString, NULL, 16);
    }

    memcpy(config->sm4_key, hex, 16);
    memset(hex, 0, sizeof(hex));
    hexString = cJSON_GetObjectItem(cfgjson, "bleMac")->valuestring;
    for (int i = 0; i < 12; i += 2) {
        char byteString[3] = {hexString[i], hexString[i + 1], '\0'};
        hex[i / 2]         = (uint8_t)strtol(byteString, NULL, 16);
    }
    memcpy(config->ble_mac, hex, 6);
    cJSON_Delete(cfgjson);
    return EOK;
}
/**
 * @file app.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-30
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdio.h>
#include <stdint.h>
#define DEVICE_ID "CYGC_001111"
// #define MQTT_URI_HOST    "172.16.248.66"
// #define MQTT_URI_PORT    "18891"
#define MQTT_URI_HOST    "broker.emqx.io"
#define MQTT_URI_PORT    "1883"
#define MQTT_TOPIC_REPLY "/iot/CYGC_001111/BR/device/reply"
#define MQTT_TOPIC_DATA  "/iot/CYGC_001111/BR/device/data"

extern uint8_t mission_status;
extern uint8_t sm4_flag;
void mission_init(void);
void mission_deinit(void);
void publish_handle(void);

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
#include "nrf_log.h"
#include "nrf_log_instance.h"

#define DEVICE_ID "CYGC_001111"
// #define MQTT_URI_HOST    "172.16.248.66"
// #define MQTT_URI_PORT    "18891"
#define MQTT_URI_HOST    "broker.emqx.io"
#define MQTT_URI_PORT    "1883"
#define MQTT_TOPIC_REPLY "/iot/CYGC_001111/BR/device/reply"
#define MQTT_TOPIC_DATA  "/iot/CYGC_001111/BR/device/data"

#if NRF_LOG_ENABLED
typedef struct {
    NRF_LOG_INSTANCE_PTR_DECLARE(p_log)
} app_t;

#define APP_LOG_DEF(_name)                           \
    NRF_LOG_INSTANCE_REGISTER(app, _name,            \
                              NRF_LOG_COLOR_DEFAULT, \
                              NRF_LOG_DEBUG_COLOR,   \
                              NRF_LOG_INITIAL_LEVEL, \
                              COMPILED_LOG_LEVEL);   \
    static app_t _name = {                           \
        NRF_LOG_INSTANCE_PTR_INIT(p_log, app, _name)};
#endif
extern uint8_t mission_status;
extern uint8_t sm4_flag;
void mission_init(void);
void mission_deinit(void);
void publish_handle(void);

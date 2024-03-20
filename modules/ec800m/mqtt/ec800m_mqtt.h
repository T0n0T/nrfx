/**
 * @file ec800m_mqtt.h
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-12-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __EC800M_MQTT_H__
#define __EC800M_MQTT_H__

#include "ec800m.h"
typedef enum {
    EC800M_MQTT_IDLE = 0,
    EC800M_MQTT_CLOSE,
    EC800M_MQTT_OPEN,
    EC800M_MQTT_CONN,
    EC800M_MQTT_DISC,
} ec800m_mqtt_status_t;
typedef struct {
    char     host[30];
    char     port[10];
    uint32_t keepalive;
    char     clientid[60];
    char     username[60];
    char     password[60];
    char     subtopic[60];
    char     pubtopic[60];
} ec800m_mqtt_t;

typedef enum {
    EC800M_TASK_MQTT_CHECK = 1,
    EC800M_TASK_MQTT_OPEN,
    EC800M_TASK_MQTT_CONNECT,
    EC800M_TASK_MQTT_PUB,
    EC800M_TASK_MQTT_SUB,
    EC800M_TASK_MQTT_CLOSE,
} ec800m_mqtt_task_t;

typedef struct {
    char*  topic;
    char*  payload;
    size_t len;
} ec800m_mqtt_data_t;

extern ec800m_mqtt_t        mqtt_config;
extern ec800m_mqtt_status_t mqtt_status;

int ec800m_mqtt_connect(void);
int ec800m_mqtt_pub(char* topic, void* payload, uint32_t len);
int ec800m_mqtt_sub(char* subtopic);
int ec800m_mqtt_conf(ec800m_mqtt_t* cfg);

#endif
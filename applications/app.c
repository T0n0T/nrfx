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
#include <rtdevice.h>
#include <stdio.h>
#include <ccm3310.h>
#include "app.h"
#include "cJSON.h"
#include "at_device_ec800x.h"
#include "paho_mqtt.h"

#define DBG_LVL DBG_INFO
#define DBG_TAG "app"
#include <rtdbg.h>

#define TIMER_DEV "timer1"
MQTTClient client;
rt_device_t timer = NULL;

char *build_msg_updata(char *device_id, struct LOC_GNSS *info, int battry, int pulse_rate);
char *build_msg_reply(char *device_id);

static rt_err_t hwtimer_call(rt_device_t dev, rt_size_t size)
{
    char *out = build_msg_updata(DEVICE_ID, ec800x_get_gnss(), 99, 75);
    paho_mqtt_publish(&client, QOS1, MQTT_TOPIC_DATA, out);
    printf("%s\n", out);
    free(out);
    return 0;
}

static int hwtimer_mission(void)
{
    rt_hwtimerval_t timeout_s = {30, 0};
    rt_hwtimer_mode_t mode    = HWTIMER_MODE_PERIOD;
    timer                     = rt_device_find(TIMER_DEV);
    if (timer == RT_NULL) {
        LOG_E("%s set callback failed!", TIMER_DEV);
        return -1;
    }
    if (rt_device_set_rx_indicate(timer, hwtimer_call)) {
        LOG_E("%s set callback failed!", TIMER_DEV);
        return -1;
    }
    if (rt_device_control(timer, HWTIMER_CTRL_MODE_SET, &mode) != RT_EOK) {
        LOG_E("%s set mode failed!", TIMER_DEV);
        return -1;
    }
    if (rt_device_write(timer, 0, &timeout_s, sizeof(timeout_s)) != sizeof(timeout_s)) {
        LOG_E("set timeout value failed\n");
        return -1;
    }
    if (rt_device_open(timer, RT_DEVICE_OFLAG_RDWR) != RT_EOK) {
        LOG_E("open %s device failed!", TIMER_DEV);
        return -1;
    }
    return 0;
}

static void mqtt_sub_callback(MQTTClient *c, MessageData *msg_data)
{
    char *out = build_msg_reply(DEVICE_ID);
    paho_mqtt_publish(&client, QOS1, MQTT_TOPIC_REPLY, out);
    printf("%s\n", out);
    free(out);
}

static int br_mqtt_init(void)
{
    rt_memset(&client, 0, sizeof(MQTTClient));
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;
    client.isconnected             = 0;
    client.uri                     = MQTT_URI;

    /* config connect param */
    rt_memcpy(&client.condata, &condata, sizeof(condata));
    client.condata.clientID.cstring  = DEVICE_ID;
    client.condata.keepAliveInterval = 30;
    client.condata.cleansession      = 1;

    /* rt_malloc buffer. */
    client.buf_size = client.readbuf_size = 1024;
    client.buf                            = rt_calloc(1, client.buf_size);
    client.readbuf                        = rt_calloc(1, client.readbuf_size);
    if (!(client.buf && client.readbuf)) {
        LOG_E("no memory for MQTT client buffer!");
        return -1;
    }

    /* set subscribe table and event callback */
    client.messageHandlers[0].topicFilter = rt_strdup(MQTT_TOPIC_REPLY);
    client.messageHandlers[0].callback    = mqtt_sub_callback;
    client.messageHandlers[0].qos         = QOS1;

    if (paho_mqtt_start(&client) != RT_EOK) {
        LOG_E("MQTT client start fail!");
    }
}
MSH_CMD_EXPORT(br_mqtt_init, test);

char *build_msg_updata(char *device_id, struct LOC_GNSS *info, int battry, int pulse_rate)
{
    cJSON *root, *body;
    char *out;
    root              = cJSON_CreateObject();
    struct timeval tv = {0};
    gettimeofday(&tv, RT_NULL);

    cJSON_AddNumberToObject(root, "timestamp", tv.tv_sec);
    cJSON_AddStringToObject(root, "type", "SmartBraceletServiceUp");
    cJSON_AddItemToObject(root, "body", body = cJSON_CreateObject());
    cJSON_AddStringToObject(body, "deviceId", device_id);
    cJSON_AddNumberToObject(body, "routePointLon", strtod(info->longitude, NULL));
    cJSON_AddNumberToObject(body, "routePointLat", strtod(info->latitude, NULL));
    // cJSON_AddNumberToObject(body, "routePointEle", strtod(info->longitude, NULL));
    cJSON_AddNumberToObject(body, "routePointEle", 0);
    cJSON_AddNumberToObject(body, "batteryLevel", battry);
    cJSON_AddNumberToObject(body, "pulseRate", pulse_rate);
    out = cJSON_Print(root);
    cJSON_Delete(root);
    return out;
}

char *build_msg_reply(char *device_id)
{
    cJSON *root, *body;
    char *out;
    root              = cJSON_CreateObject();
    struct timeval tv = {0};
    gettimeofday(&tv, RT_NULL);

    cJSON_AddStringToObject(root, "code", "200");
    cJSON_AddStringToObject(root, "msg", "OK");
    cJSON_AddNumberToObject(root, "timestamp", tv.tv_sec);
    cJSON_AddStringToObject(root, "type", "SmartBraceletServiceDown");
    cJSON_AddItemToObject(root, "body", body = cJSON_CreateObject());
    cJSON_AddStringToObject(body, "deviceId", device_id);
    out = cJSON_Print(root);
    cJSON_Delete(root);
    return out;
}

void test_json(void)
{
    char *out = build_msg_updata(DEVICE_ID, ec800x_get_gnss(), 99, 75);
    printf("%s\n", out);
    free(out);
    char *out2 = build_msg_reply(DEVICE_ID);
    printf("%s\n", out2);
    free(out2);
}
MSH_CMD_EXPORT(test_json, test);

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
#include "paho_mqtt.h"

#define DBG_LVL DBG_INFO
#define DBG_TAG "app"
#include <rtdbg.h>

static MQTTClient client;

char format_str[] = "todo";

static rt_err_t hwtimer_call(rt_device_t dev, rt_size_t size)
{
    return paho_mqtt_publish(&client, QOS1, MQTT_TOPIC_DATA, format_str);
}

static int hwtimer_mission(void)
{
    rt_hwtimer_mode_t mode = HWTIMER_MODE_PERIOD;
    rt_device_t timer      = rt_device_find("timer1");
    if (timer == RT_NULL) {
        /* todo */
        return -1;
    }
    if (rt_device_set_rx_indicate(timer, hwtimer_call)) {
        printf("%s set callback failed!", timer->parent.name);
        return -1;
    }

    if (rt_device_control(timer, HWTIMER_CTRL_MODE_SET, &mode) != RT_EOK) {
        printf("%s set mode failed!", timer->parent.name);
        return -1;
    }
    if (rt_device_open(timer, RT_DEVICE_OFLAG_RDWR) != RT_EOK) {
        printf("open %s device failed!", timer->parent.name);
        return -1;
    }
    return 0;
}

static void mqtt_sub_callback(MQTTClient *c, MessageData *msg_data)
{
    paho_mqtt_publish(&client, QOS1, MQTT_TOPIC_DATA, format_str);
}

static int br_mqtt_init(void)
{
    rt_memset(&client, 0, sizeof(MQTTClient));
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;
    static char cid[20]            = {0};
    client.isconnected             = 0;
    client.uri                     = MQTT_URI;

    /* generate the random client ID */
    rt_snprintf(cid, sizeof(cid), "CYGC_%d", rt_tick_get());
    /* config connect param */
    rt_memcpy(&client.condata, &condata, sizeof(condata));
    client.condata.clientID.cstring  = cid;
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

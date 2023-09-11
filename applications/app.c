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
#include "cJSON.h"
#include "mqttclient.h"
#include "at_device_ec800x.h"
#include <ccm3310.h>
#include <rtdevice.h>
#include <stdio.h>

#define DBG_LVL DBG_INFO
#define DBG_TAG "app"
#include <rtdbg.h>

#define MQTT_DELAY_MS 5000

/*-------thread init---------*/
uint8_t thread_isinit        = 0;
struct rt_thread mqtt_thread = {0};
char stack[2048]             = {0};

/*-------client init---------*/
mqtt_client_t *client      = NULL;
mqtt_message_t publish_msg = {0};

/*-------sm4 init---------*/
static uint8_t sm4_id      = 0;
const static uint8_t key[] = {
    0x77, 0x7f, 0x23, 0xc6,
    0xfe, 0x7b, 0x48, 0x73,
    0xdd, 0x59, 0x5c, 0xff,
    0xf6, 0x5f, 0x58, 0xec};
/*
static uint8_t cipher[] = {
    0x76, 0xf8, 0x89, 0xca,
    0x5b, 0x8f, 0xd3, 0x16,
    0xc8, 0x44, 0x84, 0x2f,
    0xe2, 0xb9, 0xe8, 0xdc,
    0xde, 0x82, 0x52, 0x91,
    0x4e, 0x87, 0x1b, 0x88,
    0x4f, 0x8b, 0x2a, 0x9f,
    0x09, 0x91, 0xd8, 0xc8,
    0xc6, 0x98, 0x57, 0xe8,
    0xa2, 0x26, 0x5d, 0x3d,
    0x78, 0xbd, 0x21, 0xcc,
    0xea, 0x5b, 0x62, 0xe3,
    0xc7, 0x8d, 0xe1, 0x2b,
    0xa8, 0xe7, 0x7c, 0x27,
    0x4c, 0x35, 0x80, 0x07,
    0x4d, 0x8d, 0x87, 0xd8,
    0x51, 0x28, 0xad, 0x2a,
    0x26, 0xf3, 0x0c, 0xb2,
    0x6a, 0xb4, 0x15, 0xde,
    0xd3, 0xa0, 0xb9, 0x3c,
    0x6f, 0x84, 0xe1, 0x0f,
    0x6c, 0x17, 0xdf, 0xc2,
    0x62, 0xed, 0x67, 0x65,
    0xff, 0xcf, 0xe3, 0x96,
    0xaa, 0xb7, 0xea, 0xd3,
    0x3b, 0x30, 0x9a, 0x87,
    0x07, 0x74, 0x5e, 0x13,
    0x37, 0x6a, 0x6d, 0x5a,
    0x0b, 0x58, 0x1a, 0xce,
    0x28, 0x8a, 0x97, 0x25,
    0x02, 0x95, 0x48, 0xec,
    0xc0, 0x6d, 0x86, 0xc7,
    0x44, 0x0f, 0x69, 0x8e,
    0xae, 0x10, 0xf7, 0x3b,
    0xa2, 0x42, 0x5f, 0xeb,
    0x73, 0x5d, 0x79, 0xce,
    0xd9, 0x63, 0xcb, 0xfc,
    0x83, 0xa3, 0x38, 0xb7,
    0x87, 0xd4, 0x8a, 0xdb,
    0xa7, 0xb7, 0x1a, 0x3c,
    0x66, 0xfb, 0x55, 0x3c,
    0x41, 0xc2, 0x40, 0x72,
    0xb3, 0x87, 0x69, 0xe4,
    0x40, 0x3b, 0x2f, 0x59,
    0xb7, 0x09, 0xee, 0x82,
    0x07, 0x9a, 0x20, 0xc9,
    0xa1, 0x12, 0xe5, 0x86,
    0xf9, 0x70, 0x7e, 0xa3,
    0x42, 0xbb, 0x4c, 0xd7,
    0x08, 0xba, 0x98, 0x55,
    0xa5, 0x3a, 0xa7, 0x84,
    0x0d, 0x5b, 0x69, 0x64,
    0xf4, 0x5e, 0x24, 0x3c,
    0x9d, 0xf4, 0x94, 0x76,
    0x20, 0x4c, 0x8b, 0x7a,
    0xf7, 0x3c, 0x55, 0x04};
*/

char *build_msg_updata(char *device_id, struct LOC_GNSS *info, int battry, int pulse_rate);
uint8_t cipher_buf[256] = {0};
int cipher_len          = 0;
static void publish_handle(void)
{
    mqtt_error_t err       = KAWAII_MQTT_SUCCESS_ERROR;
    char *publish_data     = build_msg_updata(DEVICE_ID, ec800x_get_gnss(), 99, 78);
    pdata origin_mqtt      = {rt_strlen(publish_data), (uint8_t *)publish_data};
    ciphertext cipher_mqtt = ccm3310_sm4_encrypt(sm4_id, origin_mqtt);
    memset(cipher_buf, 0, sizeof(cipher_buf));
    memcpy(cipher_buf, cipher_mqtt.data, cipher_mqtt.len);
    cipher_len = cipher_mqtt.len;
    printf("\n========= publish print encrypt ===========\n");
    if (cipher_mqtt.len >= 0) {
        for (size_t i = 0; i < cipher_mqtt.len; i++) {
            printf("%02x", *(cipher_mqtt.data + i));
        }
    }
    printf("\n===================================\n");

    if (cipher_mqtt.len > 0) {
        memset(&publish_msg, 0, sizeof(publish_msg));
        publish_msg.qos        = QOS0;
        publish_msg.payloadlen = cipher_mqtt.len;
        publish_msg.payload    = cipher_mqtt.data;
        err                    = mqtt_publish(client, MQTT_TOPIC_DATA, &publish_msg);
        if (err != KAWAII_MQTT_SUCCESS_ERROR) {
            LOG_E("publish msg fail, err[%d]", err);
            return;
        }
        LOG_D("publish msg success!!!!!!!!");
    } else {
        LOG_E("publish msg sm4 encrypt fail.");
    }
}

static void sub_handle(void *client, message_data_t *msg)
{
    (void)client;
    LOG_I("-----------------------------------------------------------------------------------");
    pdata origin_mqtt = {(int)msg->message->payloadlen, (uint8_t *)msg->message->payload};
    // pdata origin_mqtt    = {sizeof(cipher), (uint8_t *)cipher};
    plaintext plain_mqtt = ccm3310_sm4_decrypt(sm4_id, origin_mqtt);
    LOG_I("%s:%d %s()...\ntopic: %s\nmessage:%s", __FILE__, __LINE__, __FUNCTION__, msg->topic_name, (char *)plain_mqtt.data);
    LOG_I("-----------------------------------------------------------------------------------");
}

static void sub_test(void)
{
    LOG_I("\n-----------------------------------------------------------------------------------\n");
    pdata origin_mqtt    = {cipher_len, cipher_buf};
    plaintext plain_mqtt = ccm3310_sm4_decrypt(sm4_id, origin_mqtt);

    printf("\n========= subtest print decrypt ===========\n");
    if (plain_mqtt.len >= 0) {
        for (size_t i = 0; i < plain_mqtt.len; i++) {
            printf("%c", *(plain_mqtt.data + i));
        }
    }
    printf("\n===================================\n");

    LOG_I("\n-----------------------------------------------------------------------------------\n");
}
MSH_CMD_EXPORT(sub_test, test);

static void mqtt_entry(void *p)
{
    while (1) {
        publish_handle();
        rt_thread_mdelay(MQTT_DELAY_MS);
    }
}

static int thread_mission_init(void)
{
    rt_err_t err = RT_EOK;
    // memset(stack, 0, sizeof(stack));
    if (!thread_isinit) {
        err = rt_thread_init(&mqtt_thread, "app", mqtt_entry, RT_NULL, stack, sizeof(stack), 21, 10);
        if (err != RT_EOK) {
            LOG_E("mqtt thread init fail, err[%d]", err);
            return err;
        }
        thread_isinit = 1;
    }
    return rt_thread_startup(&mqtt_thread);
}

static void thread_mission_deinit(void)
{
    rt_thread_detach(&mqtt_thread);
}

int logisinit = 0;

static int mqtt_mission_init(void)
{

    mqtt_error_t err = KAWAII_MQTT_SUCCESS_ERROR;
    if (!logisinit) {
        // mqtt_log_init();
        logisinit = 1;
    }

    client = mqtt_lease();
    if (client == RT_NULL) {
        LOG_E("mqtt alloec fail");
        return -1;
    }

    mqtt_set_host(client, MQTT_URI_HOST);
    mqtt_set_port(client, MQTT_URI_PORT);
    // mqtt_set_user_name(client, "test");
    // mqtt_set_password(client, "public");
    mqtt_set_client_id(client, DEVICE_ID);
    mqtt_set_clean_session(client, 1);

    KAWAII_MQTT_LOG_I("The ID of the Kawaii client is: %s ", DEVICE_ID);

    err = mqtt_connect(client);
    if (err != KAWAII_MQTT_SUCCESS_ERROR) {
        LOG_E("mqtt connect fail, err[%d]", err);
        return -1;
    }
    rt_thread_delay(1000);
    err = mqtt_subscribe(client, MQTT_TOPIC_DATA, QOS1, sub_handle);
    if (err != KAWAII_MQTT_SUCCESS_ERROR) {
        LOG_E("mqtt set subscribe fail, err[%d]", err);
        return -1;
    }
    sm4_id = ccm3310_sm4_setkey((uint8_t *)key);
    LOG_I("mqtt app init success, keyid = 0x%02x", sm4_id);
    return 0;
}

void mission_init(void)
{
    if (mqtt_mission_init() != 0) {
        return;
    }
    // if (thread_mission_init() != 0) {
    //     mqtt_disconnect(client);
    //     mqtt_release(client);
    //     rt_free(client);
    // }
}

void mission_deinit(void)
{
    // thread_mission_deinit();
    printf("1\n");
    mqtt_disconnect(client);
    printf("2\n");
    mqtt_release(client);
    printf("3\n");
    rt_free(client);
    printf("4\n");
}

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

MSH_CMD_EXPORT_ALIAS(publish_handle, publish, test);
MSH_CMD_EXPORT(mission_init, test);
MSH_CMD_EXPORT(mission_deinit, test);

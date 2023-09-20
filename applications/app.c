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
#include "blood.h"
#include <ccm3310.h>
#include <rtdevice.h>
#include "nrf_fstorage_sd.h"

#define DBG_LVL DBG_INFO
#define DBG_TAG "app"
#include <rtdbg.h>

APP_LOG_DEF(mqtt);
APP_LOG_DEF(sm4);

#define MQTT_DELAY_MS  10000
#define FLASH_CFG_ADDR 0x60000
struct mqtt_cfg {
    char host[30];
    char port[10];
};

NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) =
    {
        /* Set a handler for fstorage events. */
        .evt_handler = NULL,

        /* These below are the boundaries of the flash space assigned to this instance of fstorage.
         * You must set these manually, even at runtime, before nrf_fstorage_init() is called.
         * The function nrf5_flash_end_addr_get() can be used to retrieve the last address on the
         * last page of flash available to write data. */
        .start_addr = FLASH_CFG_ADDR, // 开始地址
        .end_addr   = 0x7ffff,        // 结束地址
};

/*-------thread init---------*/
struct rt_thread check_thread = {0};
struct rt_thread mqtt_thread  = {0};
char stack[1024]              = {0};
char check_stack[1024]        = {0};
/*-------client init---------*/
mqtt_client_t *client      = NULL;
mqtt_message_t publish_msg = {0};
uint8_t mission_status     = 0;
/*-------sm4 init---------*/
uint8_t sm4_flag           = 0;
static uint8_t sm4_id      = 0;
const static uint8_t key[] = {
    0x77, 0x7f, 0x23, 0xc6,
    0xfe, 0x7b, 0x48, 0x73,
    0xdd, 0x59, 0x5c, 0xff,
    0xf6, 0x5f, 0x58, 0xec};

#if defined(USING_RMC)
char *build_msg_updata(char *device_id, gps_info_t info, int energyStatus, int correctlyWear);
#elif defined(USING_LOC)
char *build_msg_updata(char *device_id, struct LOC_GNSS *info, int energyStatus, int correctlyWear);
#endif

void mqtt_reset_thread(void *p);

void publish_handle(void)
{
    rt_pin_write(LED2, PIN_LOW);
    mqtt_error_t err = KAWAII_MQTT_SUCCESS_ERROR;
    // char *publish_data = build_msg_updata(DEVICE_ID, ec800x_get_gnss(), 99, g_blooddata.heart);
    char *publish_data = build_msg_updata(DEVICE_ID, ec800x_get_gnss(), 1, 1);
    if (sm4_flag) {
        pdata origin_mqtt      = {rt_strlen(publish_data), (uint8_t *)publish_data};
        ciphertext cipher_mqtt = ccm3310_sm4_encrypt(sm4_id, origin_mqtt);

        if (cipher_mqtt.len > 0) {
            memset(&publish_msg, 0, sizeof(publish_msg));
            publish_msg.qos        = QOS0;
            publish_msg.payloadlen = cipher_mqtt.len;
            publish_msg.payload    = cipher_mqtt.data;
            err                    = mqtt_publish(client, MQTT_TOPIC_DATA, &publish_msg);
            if (err != KAWAII_MQTT_SUCCESS_ERROR) {
                LOG_E("publish msg fail, err[%d]", err);
                static int flag = 0;

                if (flag++ == 10) {
                    flag = 0;
                    rt_thread_init(&check_thread, "mqtt_reset", mqtt_reset_thread, RT_NULL, check_stack, sizeof(check_stack), 25, 15);
                    rt_thread_startup(&check_thread);
                    rt_thread_mdelay(2000);
                }
            } else {
                LOG_D("publish msg success!!!!!!!!");
            }
        } else {
            LOG_E("publish msg sm4 encrypt fail.");
        }
    } else {
        memset(&publish_msg, 0, sizeof(publish_msg));
        publish_msg.qos        = QOS0;
        publish_msg.payloadlen = rt_strlen(publish_data);
        publish_msg.payload    = publish_data;
        err                    = mqtt_publish(client, MQTT_TOPIC_DATA, &publish_msg);
        if (err != KAWAII_MQTT_SUCCESS_ERROR) {
            LOG_E("publish msg fail, err[%d]", err);
        } else {
            LOG_D("publish msg success!!!!!!!!");
        }
    }
    rt_pin_write(LED2, PIN_HIGH);

    free(publish_data);
}

static void sub_handle(void *client, message_data_t *msg)
{
    rt_pin_write(LED3, PIN_LOW);
    (void)client;
    LOG_I("-----------------------------------------------------------------------------------");
    if (sm4_flag) {
        pdata origin_mqtt    = {(int)msg->message->payloadlen, (uint8_t *)msg->message->payload};
        plaintext plain_mqtt = ccm3310_sm4_decrypt(sm4_id, origin_mqtt);
        LOG_I("%s:%d %s()...\ntopic: %s\nmessage:", __FILE__, __LINE__, __FUNCTION__, msg->topic_name);
        if (plain_mqtt.len >= 0) {
            for (size_t i = 0; i < plain_mqtt.len; i++) {
                printf("%c", *(plain_mqtt.data + i));
            }
        }
    } else {
        LOG_I("%s:%d %s()...\ntopic: %s\nmessage:", __FILE__, __LINE__, __FUNCTION__, msg->topic_name);
        for (size_t i = 0; i < msg->message->payloadlen; i++) {
            printf("%c", *((char *)msg->message->payload + i));
        }
    }

    LOG_I("-----------------------------------------------------------------------------------");
    rt_pin_write(LED3, PIN_LOW);
}

int logisinit = 0;

static int mqtt_mission_init(void)
{

    mqtt_error_t err    = KAWAII_MQTT_SUCCESS_ERROR;
    ret_code_t rc       = NRF_SUCCESS;
    struct mqtt_cfg cfg = {0};
    if (nrf_fstorage_init(&fstorage, &nrf_fstorage_sd, NULL) == NRF_SUCCESS) {
        uint8_t tmp[30] = {0xff};
        if (nrf_fstorage_read(&fstorage, FLASH_CFG_ADDR, &cfg, sizeof(struct mqtt_cfg)) == NRF_SUCCESS) {
            LOG_I("read mqtt config success");
        } else {
            LOG_E("flash read fail, using default config");
            LOG_I("using default config!");
            strcpy(cfg.host, MQTT_URI_HOST);
            strcpy(cfg.port, MQTT_URI_PORT);
        }
    } else {
        LOG_E("flash init fail");
        LOG_I("using default config!");
        strcpy(cfg.host, MQTT_URI_HOST);
        strcpy(cfg.port, MQTT_URI_PORT);
    }

    LOG_I("mqtt host: %s", cfg.host);
    LOG_I("mqtt port: %s", cfg.port);
    client = mqtt_lease();
    if (client == RT_NULL) {
        LOG_E("mqtt alloec fail");
        return -1;
    }

    mqtt_set_host(client, cfg.host);
    mqtt_set_port(client, cfg.port);
    // mqtt_set_user_name(client, "test");
    // mqtt_set_password(client, "public");
    mqtt_set_client_id(client, DEVICE_ID);
    mqtt_set_clean_session(client, 1);

    KAWAII_MQTT_LOG_I("The ID of the Kawaii client is: %s ", DEVICE_ID);

    err = mqtt_connect(client);
    if (err != KAWAII_MQTT_SUCCESS_ERROR) {

        LOG_I("config invalid, use default config and write to flash");
        strcpy(cfg.host, MQTT_URI_HOST);
        strcpy(cfg.port, MQTT_URI_PORT);
        nrf_fstorage_erase(&fstorage, FLASH_CFG_ADDR, 1, NULL);
        if (nrf_fstorage_write(&fstorage, FLASH_CFG_ADDR, &cfg, sizeof(struct mqtt_cfg), NULL) != NRF_SUCCESS) {
            LOG_E("write config fail");
            return -1;
        }
        NRFX_DELAY_US(1000000); // wait for write finish
        err = mqtt_connect(client);
        if (err != KAWAII_MQTT_SUCCESS_ERROR) {
            LOG_E("mqtt connect fail, err[%d]", err);
            return -1;
        }
    }
    rt_thread_delay(1000);
    err = mqtt_subscribe(client, MQTT_TOPIC_REPLY, QOS1, sub_handle);
    if (err != KAWAII_MQTT_SUCCESS_ERROR) {
        LOG_E("mqtt set subscribe fail, err[%d]", err);
        return -1;
    }
    sm4_id = ccm3310_sm4_setkey((uint8_t *)key);
    if (!sm4_id) {
        sm4_id = ccm3310_sm4_updatekey_ram(0x81, (uint8_t *)key);
        if (!sm4_id) {
            LOG_W("mqtt app init success, but cannot get new keyid, work without sm4");
            sm4_flag = 0;
        } else {
            LOG_W("mqtt app init success, updating and using keyid 0x81");
            sm4_flag = 1;
        }

    } else {
        LOG_I("mqtt app init success, keyid = 0x%02x", sm4_id);
        sm4_flag = 1;
    }

    return 0;
}

static void mqtt_entry(void *p)
{
    while (ec800x_isinit() == RT_FALSE) {
        rt_thread_mdelay(3000);
    }
    rt_thread_mdelay(1000);
    rt_pin_write(LED2, PIN_LOW);
    rt_pin_write(LED3, PIN_LOW);
    rt_thread_mdelay(1000);
    rt_pin_write(LED2, PIN_HIGH);
    rt_pin_write(LED3, PIN_HIGH);

    if (mqtt_mission_init() != 0) {
        rt_thread_init(&check_thread, "mqtt_reset", mqtt_reset_thread, RT_NULL, check_stack, sizeof(check_stack), 25, 15);
        rt_thread_startup(&check_thread);
        rt_thread_mdelay(2000);
        return;
    }

    rt_thread_mdelay(1000);
    rt_pin_write(LED2, PIN_LOW);
    rt_pin_write(LED3, PIN_LOW);
    rt_thread_mdelay(1000);
    rt_pin_write(LED2, PIN_HIGH);
    rt_pin_write(LED3, PIN_HIGH);

    while (1) {
        publish_handle();
        rt_thread_mdelay(MQTT_DELAY_MS);
    }
}

void mission_init(void)
{
    if (rt_thread_init(&mqtt_thread, "app", mqtt_entry, RT_NULL, stack, sizeof(stack), 21, 10) != RT_EOK) {
        LOG_E("mqtt thread init fail");
        return;
    }
    rt_thread_startup(&mqtt_thread);

    mission_status = 1;
}

void mission_deinit(void)
{
    if (rt_thread_detach(&mqtt_thread) != RT_EOK) {
        printf("deinit app fail\n");
        return;
    }
    printf("1\n");
    mqtt_disconnect(client);
    printf("2\n");
    mqtt_release(client);
    printf("3\n");
    rt_free(client);
    printf("4\n");
    mission_status = 0;
}

void mqtt_reset_thread(void *p)
{
    mission_deinit();
    rt_thread_mdelay(2000);
    mission_init();
}

#if defined(USING_RMC)
char *build_msg_updata(char *device_id, gps_info_t info, int energyStatus, int correctlyWear)
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
#elif defined(USING_LOC)
char *build_msg_updata(char *device_id, struct LOC_GNSS *info, int energyStatus, int correctlyWear)
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
    cJSON_AddNumberToObject(body, "energyStatus", energyStatus);
    cJSON_AddNumberToObject(body, "correctlyWear", correctlyWear);
    out = cJSON_Print(root);
    cJSON_Delete(root);
    return out;
}
#endif

void sm4flag(int argc, char **argv)
{
    if (argc < 2) {
        printf("sm4flag on/off      - using cmd to change mode of sm4\n");
        return;
    }
    if (!rt_strncmp("on", argv[1], 3)) {
        printf("sm4 encrypt mode ON !!!!!!!!!\n");
        sm4_flag = 1;
    } else if (!rt_strncmp("off", argv[1], 3)) {
        printf("sm4 encrypt mode OFF!!!!!!!!!\n");
        sm4_flag = 0;
    }
}

void sm4_setkey(int argc, char **argv)
{
    if (argc < 18) {
        printf("sm4_key set/update xx[0] xx[1] xx[2]... xx[15]   - using hex key\n");
    }
    uint8_t key[16] = {0};
    printf("argv[1] = %s", argv[1]);
    for (size_t i = 0; i < 16; i++) {
        key[i] = strtol(argv[2 + i], NULL, 16);
    }
    if (!strncmp("set", argv[1], 3)) {
        printf("set new key!!!!!\n");
        uint8_t id = ccm3310_sm4_setkey(key);
        if (!id) {
            printf("set sm4 key fail! no id to allocate\n");
            return;
        }
        sm4_id = id;
        printf("set sm4 key success! key_id = 0x%02x\n", sm4_id);
    } else if (!strncmp("update", argv[1], 6)) {

        uint8_t update_id = 0;
        if (sm4_id) {
            printf("update key 0x%02x!!!!!\n", sm4_id);
            update_id = sm4_id;
        } else {
            printf("update key 0x81!!!!!\n");
            update_id = 0x81; // 0x81 is the default key id
        }

        uint8_t id = ccm3310_sm4_updatekey_ram(update_id, key);
        if (!id) {
            printf("update sm4 key fail!");
            return;
        }
        sm4_id = id;
        printf("update sm4 key success! key_id = 0x%02x\n", sm4_id);
    }
}

static void set_mqtt(int argc, char **argv)
{
    if (argc < 3) {
        printf("set_mqtt [host] [ip]   - setting mqtt url and save in flash\n");
    }
    struct mqtt_cfg cfg = {0};

    strcpy(cfg.host, argv[1]);
    strcpy(cfg.port, argv[2]);
    printf("host:%s\n", cfg.host);
    printf("port:%s\n", cfg.port);

    nrf_fstorage_erase(&fstorage, FLASH_CFG_ADDR, 1, NULL);
    if (nrf_fstorage_write(&fstorage, FLASH_CFG_ADDR, &cfg, sizeof(struct mqtt_cfg), NULL) != NRF_SUCCESS) {
        LOG_E("write config fail");
    }
    NRFX_DELAY_US(1000000); // wait for write finish
    struct mqtt_cfg *after_cfg = 0;
    after_cfg                  = (struct mqtt_cfg *)FLASH_CFG_ADDR;
    printf("host:%s\n", after_cfg->host);
    printf("port:%s\n", after_cfg->port);
    printf("config write success!\n");
    printf("restart machine to make config update\n");
}

MSH_CMD_EXPORT_ALIAS(publish_handle, publish, test);
MSH_CMD_EXPORT(mission_init, test);
MSH_CMD_EXPORT(mission_deinit, test);
MSH_CMD_EXPORT(sm4flag, test);
MSH_CMD_EXPORT(sm4_setkey, setkey with input);
MSH_CMD_EXPORT(set_mqtt, setkey with input);
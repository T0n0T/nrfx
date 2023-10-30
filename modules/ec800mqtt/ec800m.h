/**
 * @file ec800m.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-26
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef _EC800M_H_
#define _EC800M_H_

#include "at.h"
#include "gps_rmc.h"
#include "event_groups.h"

#define EC800M_RESET_MAX        10
#define EC800M_BUF_LEN          256
#define AT_CLIENT_RECV_BUFF_LEN 128

/** ec800m event */
#define EC800M_EVENT_IDLE          (1 << 0)
#define EC800M_EVENT_MQTT_RELEASE  (1 << 1)
#define EC800M_EVENT_MQTT_CONNECT  (1 << 2)
#define EC800M_EVENT_MQTT_SUBCIRBE (1 << 3)
#define EC800M_EVENT_GNSS_FULSH    (1 << 4)

#define EC800M_MQTT_DEFAULT_CFG                        \
    {                                                  \
        .host      = "broker.emqx.io",                 \
        .port      = "1883",                           \
        .keepalive = 300,                              \
        .clientid  = "CYGC_TEST",                      \
        .username  = NULL,                             \
        .password  = NULL,                             \
        .subtopic  = "/iot/CYGC_TEST/BR/device/reply", \
        .pubtopic  = "/iot/CYGC_TEST/BR/device/data",  \
    }

typedef enum {
    EC800M_IDLE = 0,
    EC800M_MQTT_CLOSE,
    EC800M_MQTT_OPEN,
    EC800M_MQTT_CONN,
    EC800M_MQTT_DISC,
} ec800m_status_t;

typedef struct {
    char*    host;
    char*    port;
    uint32_t keepalive;
    char*    clientid;
    char*    username;
    char*    password;
    char*    subtopic;
    char*    pubtopic;
} ec800m_mqtt_t;

typedef struct {
    at_client_t        client;
    uint32_t           pwr_pin;
    uint32_t           wakeup_pin;
    ec800m_mqtt_t      mqtt;
    ec800m_status_t    status;
    EventGroupHandle_t event;
    TimerHandle_t      timer;
    int                rssi;
    uint8_t            reset_need;
} ec800m_t;

#pragma pack(8)
struct at_cmd {
    char*   desc;
    char*   cmd_expr;
    char*   resp_keyword;
    int32_t timeout;
};
typedef struct at_cmd* at_cmd_t;
#pragma pack()

#define AT_CMD(NUM)          (&at_cmd_list[NUM])
#define AT_CMD_NAME(CMD_NUM) (#CMD_NUM)
typedef enum {
    /** common */
    AT_CMD_TEST,
    AT_CMD_RESET,
    AT_CMD_ECHO_OFF,
    AT_CMD_LOW_POWER,

    /** simcard */
    AT_CMD_IMSI,
    AT_CMD_ICCID,
    AT_CMD_GSN,
    AT_CMD_CHECK_SIGNAL,
    AT_CMD_CHECK_GPRS,
    AT_CMD_CHECK_PIN,

    /** tcp/ip */
    AT_CMD_ACT_PDP,
    AT_CMD_DEACT_PDP,
    AT_CMD_DATAECHO_OFF,
    AT_CMD_PING,
    AT_CMD_QUERY_TARGET_IP,
    AT_CMD_QUERY_OWN_IP,
    AT_CMD_CONF_DNS,
    AT_CMD_QUERY_DNS,

    /** socket */
    AT_CMD_SOCKET_OPEN,
    AT_CMD_SOCKET_CLOSE,
    AT_CMD_SOCKET_SEND,
    AT_CMD_SOCKET_RECV,
    AT_CMD_SOCKET_STATUS,

    /** gnss */
    AT_CMD_GNSS_OPEN,
    AT_CMD_GNSS_CLOSE,
    AT_CMD_GNSS_STATUS,
    AT_CMD_GNSS_LOC,
    AT_CMD_GNSS_NMEA_CONF,
    AT_CMD_GNSS_NMEA_RMC,

    /** mqtt */
    AT_CMD_MQTT_REL,
    AT_CMD_MQTT_CHECK_REL,
    AT_CMD_MQTT_CLOSE,
    AT_CMD_MQTT_STATUS,
    AT_CMD_MQTT_CONNECT,
    AT_CMD_MQTT_DISCONNECT,
    AT_CMD_MQTT_PUBLISH,
    AT_CMD_MQTT_SUBSCRIBE,
    AT_CMD_MQTT_READBUF,
    AT_CMD_MQTT_CONF_ALIVE,

    AT_CMD_MAX,
} at_cmd_desc_t;

extern ec800m_t            ec800m;
extern const struct at_cmd at_cmd_list[];

void       ec800m_init(void);
int        ec800m_mqtt_connect(void);
int        ec800m_mqtt_pub(char* topic, char* payload);
int        ec800m_mqtt_sub(char* subtopic);
int        ec800m_mqtt_conf(ec800m_mqtt_t* cfg);
gps_info_t ec800m_gnss_get(void);

#endif
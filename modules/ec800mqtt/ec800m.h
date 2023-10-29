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
#include "event_groups.h"

#define EC800M_BUF_LEN          64
#define AT_CLIENT_RECV_BUFF_LEN 128

/** ec800m event */
#define EC800M_EVENT_MQTT_IDLE     (1 << 0)
#define EC800M_EVENT_MQTT_RELEASE  (1 << 1)
#define EC800M_EVENT_MQTT_CONNECT  (1 << 2)
#define EC800M_EVENT_MQTT_ALIVE    (1 << 3)
#define EC800M_EVENT_MQTT_SUBCIRBE (1 << 4)
#define EC800M_EVENT_MQTT_PUBLISH  (1 << 5)
#define EC800M_EVENT_GNSS_CONF     (1 << 6)
#define EC800M_EVENT_GNSS_OPEN     (1 << 7)
#define EC800M_EVENT_GNSS_FULSH    (1 << 8)

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
    bool               waiting;
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

#define AT_STRERROR_ENTITY(desc)    \
    {                               \
        .code = desc, .name = #desc \
    }
typedef struct
{
    uint32_t    code;
    char const* name;
} at_cmd_strerror_t;

extern ec800m_t            ec800m;
extern const struct at_cmd at_cmd_list[];

int ec800m_init(void);

#endif
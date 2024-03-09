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
#include "timers.h"
#include "event_groups.h"
#include "queue.h"
#include "ec800m_mqtt.h"
#include "ec800m_socket.h"

#define EC800_MQTT              0x01
#define EC800_SOCKET            0x02
#define EC800M_RESET_MAX        5
#define EC800M_BUF_LEN          256
#define EC800M_TASK_MAX_NUM     3
#define AT_CLIENT_RECV_BUFF_LEN 128

typedef enum {
    EC800M_IDLE = 1,
    EC800M_BUSY,
    EC800M_ERROR,
} ec800m_status_t;

typedef struct {
    uint32_t type;
    uint32_t task;
    void*    param;
    uint32_t timeout;
} ec800m_task_t;

typedef struct {
    uint8_t id;
    void    (*init)(void);
    void    (*task_handle)(int, void*);
    void    (*timeout_handle)(int, void*);
} ec800m_task_group_t;

typedef struct {
    at_client_t     client;
    uint32_t        pwr_pin;
    uint32_t        wakeup_pin;
    QueueHandle_t   task_queue;
    TimerHandle_t   timer;
    ec800m_status_t status;
    int             rssi;
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
    AT_CMD_LOW_POWER_OFF,

    /** simcard */
    AT_CMD_IMSI,
    AT_CMD_ICCID,
    AT_CMD_GSN,
    AT_CMD_CHECK_SIGNAL,
    AT_CMD_CHECK_GPRS,
    AT_CMD_CHECK_PIN,

    /** tcp/ip */
    AT_CMD_ACT_PDP,
    AT_CMD_CHECK_PDP,
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

    AT_CMD_ERR_CHECK,
    AT_CMD_POWER_DOWN,
    AT_CMD_MAX,
} at_cmd_desc_t;

extern ec800m_t            ec800m;
extern const struct at_cmd at_cmd_list[];

void       ec800m_init(void);
int        at_cmd_exec(at_client_t dev, char* prase_buf, at_cmd_desc_t at_cmd_id, ...);
void       ec800m_wake_up(void);
int        ec800m_power_on(void);
void       ec800m_power_off(void);
gps_info_t ec800m_gnss_get(void);

#endif
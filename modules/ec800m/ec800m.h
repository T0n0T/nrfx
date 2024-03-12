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
#include "queue.h"
#include "semphr.h"

#define EC800_COMM           0x00
#define EC800_MQTT           0x01
#define EC800_SOCKET         0x02
#define EC800M_RESET_MAX     5
#define EC800M_BUF_LEN       256
#define EC800M_TASK_MAX_NUM  3
#define EC800M_RECV_BUFF_LEN 128
#define EC800M_IPC_MIN_TICK  50
typedef enum {
    EC800M_POWER_OFF = 1,
    EC800M_POWER_ON,
    EC800M_POWER_LOW,
} ec800m_status_t;

typedef struct {
    uint32_t type;
    uint32_t task;
    void*    param;
} ec800m_task_t;

typedef struct {
    at_client_t       client;
    uint32_t          pwr_pin;
    uint32_t          wakeup_pin;
    QueueHandle_t     task_queue;
    SemaphoreHandle_t sync; ///< both two ways given from at_client.c, a sync for some cmd executing is necessary for user thread.
    SemaphoreHandle_t lock; ///< in order to make 'one task only', 'before cmd' and 'after cmd' need lock and unlock.
    ec800m_status_t   status;
    int               err;
} ec800m_t;

typedef struct {
    uint8_t id;
    void    (*init)(ec800m_t*);
    void    (*task_handle)(ec800m_task_t*, ec800m_t*);
    void    (*err_handle)(ec800m_task_t*, ec800m_t*);
} ec800m_task_group_t;

#pragma pack(8)
struct at_cmd {
    char*    desc;
    char*    cmd_expr;
    char*    resp_keyword;
    uint32_t resp_linenum;
    uint32_t timeout;
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

extern const struct at_cmd at_cmd_list[];

ec800m_t* ec800m_init(void);
int       ec800m_wait_sync(ec800m_t* dev, uint32_t timeout);
void      ec800m_post_sync(ec800m_t* dev);
int       at_cmd_exec(at_client_t dev, at_cmd_desc_t at_cmd_id, char* keyword_line, ...);

#endif
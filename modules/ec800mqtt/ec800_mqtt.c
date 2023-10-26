/*
 * Copyright (c) 2023, RudyLo <luhuadong@163.com>
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-08     luhuadong    the first version
 * 2020-06-04     luhuadong    v0.0.1
 * 2020-07-25     luhuadong    support state transition
 * 2020-08-16     luhuadong    support bind recv parser
 * 2023-03-28     kurisaW      support serial v2
 */
#include <at.h>
#include "ec800_mqtt.h"

#define NRF_LOG_MODULE_NAME ec800mqtt
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define EC800_ADC0_PIN             -1
#define EC800_RESET_N_PIN          -1
#define EC800_OP_BAND              8

#define PRODUCT_KEY                ""
#define DEVICE_NAME                ""
#define DEVICE_SECRET              ""

#define KEEP_ALIVE_TIME            300

#define AT_OK                      "OK"
#define AT_ERROR                   "ERROR"

#define AT_TEST                    "AT"
#define AT_ECHO_OFF                "ATE0"
#define AT_QREGSWT_2               "AT+QREGSWT=2"
#define AT_AUTOCONNECT_DISABLE     "AT+NCONFIG=AUTOCONNECT,FALSE"
#define AT_REBOOT                  "AT+NRB"
#define AT_NBAND                   "AT+NBAND=%d"
#define AT_FUN_ON                  "AT+CFUN=1"
#define AT_LED_ON                  "AT+QLEDMODE=1"
#define AT_EDRX_OFF                "AT+CEDRXS=0,5"
#define AT_PSM_OFF                 "AT+CPSMS=0"
#define AT_RECV_AUTO               "AT+NSONMI=2"
#define AT_UE_ATTACH               "AT+CGATT=1"
#define AT_UE_DEATTACH             "AT+CGATT=0"
#define AT_QUERY_IMEI              "AT+CGSN=1"
#define AT_QUERY_IMSI              "AT+CIMI"
#define AT_QUERY_STATUS            "AT+NUESTATS"
#define AT_QUERY_REG               "AT+CEREG?"
#define AT_QUERY_IPADDR            "AT+CGPADDR"
#define AT_QUERY_ATTACH            "AT+CGATT?"
#define AT_UE_ATTACH_SUCC          "+CGATT:1"

#define AT_MQTT_AUTH               "AT+QMTCFG=\"aliauth\",0,\"%s\",\"%s\",\"%s\""
#define AT_MQTT_ALIVE              "AT+QMTCFG=\"keepalive\",0,%u"
#define AT_MQTT_OPEN               "AT+QMTOPEN=0,\"%s\",%s"
#define AT_MQTT_OPEN_SUCC          "+QMTOPEN: 0,0"
#define AT_MQTT_CLOSE              "AT+QMTCLOSE=0"
#define AT_MQTT_CONNECT            "AT+QMTCONN=0,\"%s\""
#define AT_MQTT_CONNECT_SUCC       "+QMTCONN: 0,0,0"
#define AT_MQTT_DISCONNECT         "AT+QMTDISC=0"
#define AT_MQTT_SUB                "AT+QMTSUB=0,1,\"%s\",2"
#define AT_MQTT_SUB_SUCC           "+QMTSUB: 0,1,0,0"
#define AT_MQTT_UNSUB              "AT+QMTUNS=0,1, \"%s\""
#define AT_MQTT_PUB                "AT+QMTPUB=0,0,0,0,\"%s\",%d"
#define AT_MQTT_PUB_SUCC           "+QMTPUB: 0,0,0"
#define AT_MQTT_PUBEX              "AT+QMTPUBEX=0,0,0,0,\"%s\",%d"
#define AT_MQTT_PUBEX_SUCC         "+QMTPUBEX: 0,0,0"

#define AT_QMTSTAT_CLOSED          1
#define AT_QMTSTAT_PINGREQ_TIMEOUT 2
#define AT_QMTSTAT_CONNECT_TIMEOUT 3
#define AT_QMTSTAT_CONNACK_TIMEOUT 4
#define AT_QMTSTAT_DISCONNECT      5
#define AT_QMTSTAT_WORNG_CLOSE     6
#define AT_QMTSTAT_INACTIVATED     7

#define AT_QMTRECV_DATA            "+QMTRECV: %d,%d,\"%s\",\"%s\""

#define EC800_AT_CMD_OPEN_GNSS     "AT+QGPS=1"
#define EC800_AT_CMD_GNSS_QUERY    "AT+QGPS?"
#define EC800_AT_CMD_GET_GPS_LOC   "AT+QGPSLOC=2"
#define EC800_AT_CMD_NEMA_GGA      "AT+QGPSGNMEA=\"GGA\""
#define EC800_AT_CMD_NEMA_RMC      "AT+QGPSGNMEA=\"RMC\""
#define EC800_AT_CMD_QUERY_CSQ     "AT+CSQ"

#define AT_CLIENT_RECV_BUFF_LEN    128
#define AT_DEFAULT_TIMEOUT         300

struct ec800_device ec800 = {
    .event     = NULL,
    .reset_pin = EC800_RESET_N_PIN,
    .adc_pin   = EC800_ADC0_PIN,
    .stat      = EC800_STAT_DISCONNECTED};

static struct ec800_connect_obj ec800_connect = {
    .host = "129.226.207.116",
    .port = "1883"};

static char buf[AT_CLIENT_RECV_BUFF_LEN];

void ec800_subscribe_callback(const char *json);

/**
 * This function will send command and check the result.
 *
 * @param client    at client handle
 * @param cmd       command to at client
 * @param resp_expr expected response expression
 * @param lines     response lines
 * @param timeout   waiting time
 *
 * @return  EOK       send success
 *         -ERROR     send failed
 *         -ETIMEOUT  response timeout
 *         -ENOMEM    alloc memory failed
 */
static int check_send_cmd(const char *cmd, const char *resp_expr,
                          const size_t lines, const int32_t timeout)
{
    at_response_t resp            = NULL;
    int result                    = 0;
    char resp_arg[AT_CMD_MAX_LEN] = {0};

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, lines, timeout);
    if (resp == NULL) {
        NRF_LOG_ERROR("No memory for response structure!");
        return -ENOMEM;
    }

    result = at_obj_exec_cmd(ec800.client, resp, cmd);
    if (result < 0) {
        NRF_LOG_ERROR("AT client send commands failed or wait response timeout!");
        goto __exit;
    }

    if (resp_expr) {
        if (at_resp_parse_line_args_by_kw(resp, resp_expr, "%s", resp_arg) <= 0) {
            NRF_LOG_DEBUG("# >_< Failed");
            result = -ERROR;
            goto __exit;
        } else {
            NRF_LOG_DEBUG("# ^_^ successed");
        }
    }

    result = EOK;

__exit:
    if (resp) {
        at_delete_resp(resp);
    }

    return result;
}

static char *ec800_get_imei(ec800_device_t device)
{
    at_response_t resp = NULL;

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, 300);
    if (resp == NULL) {
        NRF_LOG_ERROR("No memory for response structure!");
        return NULL;
    }

    /* send "AT+CGSN=1" commond to get device IMEI */
    if (at_obj_exec_cmd(device->client, resp, AT_QUERY_IMEI) != EOK) {
        at_delete_resp(resp);
        return NULL;
    }

    if (at_resp_parse_line_args(resp, 2, "+CGSN:%s", device->imei) <= 0) {
        NRF_LOG_ERROR("device parse \"%s\" cmd error.", AT_QUERY_IMEI);
        at_delete_resp(resp);
        return NULL;
    }
    NRF_LOG_DEBUG("IMEI code: %s", device->imei);

    at_delete_resp(resp);
    return device->imei;
}

static char *ec800_get_ipaddr(ec800_device_t device)
{
    at_response_t resp = NULL;

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, 300);
    if (resp == NULL) {
        NRF_LOG_ERROR("No memory for response structure!");
        return NULL;
    }

    /* send "AT+CGPADDR" commond to get IP address */
    if (at_obj_exec_cmd(device->client, resp, AT_QUERY_IPADDR) != EOK) {
        at_delete_resp(resp);
        return NULL;
    }

    /* parse response data "+CGPADDR: 0,<IP_address>" */
    if (at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR:%*d,%s", device->ipaddr) <= 0) {
        NRF_LOG_ERROR("device parse \"%s\" cmd error.", AT_QUERY_IPADDR);
        at_delete_resp(resp);
        return NULL;
    }
    NRF_LOG_DEBUG("IP address: %s", device->ipaddr);

    at_delete_resp(resp);
    return device->ipaddr;
}

static int ec800_mqtt_set_alive(rt_uint32_t keepalive_time)
{
    NRF_LOG_DEBUG("MQTT set alive.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    sprintf(cmd, AT_MQTT_ALIVE, keepalive_time);

    return check_send_cmd(cmd, AT_OK, 0, 1000);
}

int ec800_mqtt_auth(void)
{
    NRF_LOG_DEBUG("MQTT set auth info.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    sprintf(cmd, AT_MQTT_AUTH, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

/**
 * Open MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int ec800_mqtt_open(void)
{
    NRF_LOG_DEBUG("MQTT open socket.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    char *str                = NULL;
    char ip[16]              = {0};
    char port[8]             = {0};

    // str = read_config_from_file("config.txt", "ip");
    // str ? memcpy(ip, str, strlen(str)) : memcpy(ip, ec800_connect.ip, strlen(ec800_connect.ip));

    // str = read_config_from_file("config.txt", "port");
    // str ? memcpy(port, str, strlen(str)) : memcpy(port, ec800_connect.port, strlen(ec800_connect.port));

    sprintf(cmd, AT_MQTT_OPEN, ip, port);

    return check_send_cmd(cmd, AT_MQTT_OPEN_SUCC, 4, 3000);
}

/**
 * Close MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int ec800_mqtt_close(void)
{
    NRF_LOG_DEBUG("MQTT close socket.");

    return check_send_cmd(AT_MQTT_CLOSE, AT_OK, 0, 3000);
}

/**
 * Connect MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int ec800_mqtt_connect(void)
{
    NRF_LOG_DEBUG("MQTT connect...");

    char cmd[AT_CMD_MAX_LEN] = {0};
    // sprintf(cmd, AT_MQTT_CONNECT, ec800.imei);
    sprintf(cmd, AT_MQTT_CONNECT, "1");
    NRF_LOG_DEBUG("%s", cmd);

    if (check_send_cmd(cmd, AT_MQTT_CONNECT_SUCC, 4, 10000) < 0) {
        NRF_LOG_DEBUG("MQTT connect failed.");
        return -ERROR;
    }

    ec800.stat = EC800_STAT_CONNECTED;
    return EOK;
}

/**
 * Disconnect MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int ec800_mqtt_disconnect(void)
{
    if (check_send_cmd(AT_MQTT_DISCONNECT, AT_OK, 0, AT_DEFAULT_TIMEOUT) < 0) {
        NRF_LOG_DEBUG("MQTT disconnect failed.");
        return -ERROR;
    }

    ec800.stat = EC800_STAT_CONNECTED;
    return EOK;
}

/**
 * Subscribe MQTT topic.
 *
 * @param  topic : mqtt topic
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int ec800_mqtt_subscribe(const char *topic)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    sprintf(cmd, AT_MQTT_SUB, topic);

    return check_send_cmd(cmd, AT_MQTT_SUB_SUCC, 4, AT_DEFAULT_TIMEOUT);
}

/**
 * Unsubscribe MQTT topic.
 *
 * @param  topic : mqtt topic
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int ec800_mqtt_unsubscribe(const char *topic)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    sprintf(cmd, AT_MQTT_UNSUB, topic);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

/**
 * Publish MQTT message to topic.
 *
 * @param  topic : mqtt topic
 * @param  msg   : message
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int ec800_mqtt_publish(const char *topic, const char *msg)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    // sprintf(cmd, AT_MQTT_PUB, topic, strlen(msg));
    sprintf(cmd, AT_MQTT_PUBEX, topic, strlen(msg));

    /* set AT client end sign to deal with '>' sign.*/
    at_set_end_sign('>');

    check_send_cmd(cmd, ">", 0, 1000);
    NRF_LOG_DEBUG("publish...");

    /* reset the end sign for data conflict */
    at_set_end_sign(0);

    check_send_cmd(msg, AT_MQTT_PUB_SUCC, 4, AT_DEFAULT_TIMEOUT);
    // at_obj_exec_cmd(ec800.client, NULL, AT_MQTT_PUB_SUCC);
    return EOK;
}

/**
 * Attach EC800 device to network.
 *
 * @return 0 : attach success
 *        <0 : attach failed
 */
int ec800_client_attach(void)
{
    int result               = 0;
    char cmd[AT_CMD_MAX_LEN] = {0};

    while (EOK != check_send_cmd("AT", AT_OK, 0, AT_DEFAULT_TIMEOUT)) {
        rt_thread_mdelay(1000);
        rt_kprintf("AT no responend\r\n");
    }

    rt_kprintf("AT responend OK\r\n");

    /* close echo */
    check_send_cmd(AT_ECHO_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);

    /* 关闭电信自动注册功能 */
    result = check_send_cmd(AT_QREGSWT_2, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 禁用自动连接网络 */
    result = check_send_cmd(AT_AUTOCONNECT_DISABLE, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 重启模块 */
    check_send_cmd(AT_REBOOT, AT_OK, 0, 10000);

    while (EOK != check_send_cmd(AT_TEST, AT_OK, 0, AT_DEFAULT_TIMEOUT)) {
        rt_thread_mdelay(1000);
        rt_kprintf("AT no responend\r\n");
    }

    rt_kprintf("AT responend OK\r\n");

    /* 查询IMEI号 */
    result = check_send_cmd(AT_QUERY_IMEI, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    if (NULL == ec800_get_imei(&ec800)) {
        NRF_LOG_ERROR("Get IMEI code failed.");
        return -ERROR;
    }

    /* 指定要搜索的频段 */
    sprintf(cmd, AT_NBAND, EC800_OP_BAND);
    result = check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 打开模块的调试灯 */
    // result = check_send_cmd(AT_LED_ON, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    // if (result != EOK) return result;

    /* 将模块设置为全功能模式(开启射频功能) */
    result = check_send_cmd(AT_FUN_ON, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 接收到TCP数据时，自动上报 */
    result = check_send_cmd(AT_RECV_AUTO, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 关闭eDRX */
    result = check_send_cmd(AT_EDRX_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 关闭PSM */
    result = check_send_cmd(AT_PSM_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 查询卡的国际识别码(IMSI号)，用于确认SIM卡插入正常 */
    result = check_send_cmd(AT_QUERY_IMSI, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 触发网络连接 */
    result = check_send_cmd(AT_UE_ATTACH, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != EOK) return result;

    /* 查询模块状态 */
    // at_exec_cmd(resp, "AT+NUESTATS");

    /* 查询网络注册状态 */
    // at_exec_cmd(resp, "AT+CEREG?");

    /* 查看信号强度 */
    // at_exec_cmd(resp, "AT+CSQ");

    /* 查询网络是否被激活，通常需要等待30s */
    int count = 60;
    while (count > 0 && EOK != check_send_cmd(AT_QUERY_ATTACH, AT_UE_ATTACH_SUCC, 0, AT_DEFAULT_TIMEOUT)) {
        rt_thread_mdelay(1000);
        count--;
    }

    /* 查询模块的 IP 地址 */
    // at_exec_cmd(resp, "AT+CGPADDR");
    ec800_get_ipaddr(&ec800);

    if (count > 0) {
        ec800.stat = EC800_STAT_ATTACH;
        return EOK;
    } else {
        return -ETIMEOUT;
    }
}

/**
 * Deattach EC800 device from network.
 *
 * @return 0 : deattach success
 *        <0 : deattach failed
 */
int ec800_client_deattach(void)
{
    int result = 0;
    result     = check_send_cmd(AT_UE_DEATTACH, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (EOK != result) {
        return -ERROR;
    }

    ec800.stat = EC800_STAT_DEATTACH;
    return EOK;
}

/**
 * AT client initialize.
 *
 * @return 0 : initialize success
 *        -1 : initialize failed
 *        -5 : no memory
 */
static int at_client_dev_init(void)
{
    int result = EOK;

    /* initialize AT client */
    result = at_client_init("ec800m", AT_CLIENT_RECV_BUFF_LEN);
    if (result < 0) {
        NRF_LOG_ERROR("at client ec800m init failed.");
        return result;
    }

    ec800.client = at_client_get(AT_CLIENT_DEV_NAME);
    if (ec800.client == NULL) {
        NRF_LOG_ERROR("get AT client ec800m failed.");
        return -ERROR;
    }

    return EOK;
}

void ec800_reset(void)
{
}

int at_client_port_init(void);

/**
 * EC800 device initialize.
 *
 * @return 0 : initialize success
 *        -1 : initialize failed
 */
int ec800_init(void)
{
    NRF_LOG_DEBUG("Init at client device.");
    at_client_dev_init();
    at_client_port_init();

    NRF_LOG_DEBUG("Reset EC800 device.");
    ec800_reset();

    ec800.stat = EC800_STAT_INIT;
    ec800_bind_parser(ec800_subscribe_callback);

    return EOK;
}

void ec800_bind_parser(void (*callback)(const char *json))
{
    ec800.parser = callback;
}

int ec800_build_mqtt_network(void)
{
    int result = 0;

    ec800_mqtt_set_alive(KEEP_ALIVE_TIME);

    //    if((result = ec800_mqtt_auth()) < 0) {
    //        return result;
    //    }

    if ((result = ec800_mqtt_open()) < 0) {
        NRF_LOG_ERROR("MQTT open failed.");
        return result;
    }

    if ((result = ec800_mqtt_connect()) < 0) {
        NRF_LOG_ERROR("MQTT connect failed.");
        return result;
    }

    return EOK;
}

int ec800_rebuild_mqtt_network(void)
{
    int result = 0;

    ec800_mqtt_close();
    ec800_mqtt_set_alive(KEEP_ALIVE_TIME);

    //    if((result = ec800_mqtt_auth()) < 0) {
    //        return result;
    //    }

    if ((result = ec800_mqtt_open()) < 0) {
        return result;
    }

    if ((result = ec800_mqtt_connect()) < 0) {
        return result;
    }

    return EOK;
}

static int ec800_deactivate_pdp(void)
{
    /* AT+CGACT=<state>,<cid> */

    return EOK;
}

static void urc_mqtt_stat(struct at_client *client, const char *data, size_t size)
{
    /* MQTT链路层的状态发生变化 */
    NRF_LOG_DEBUG("The state of the MQTT link layer changes");
    NRF_LOG_DEBUG("%s", data);

    int tcp_conn_id = 0, err_code = 0;
    sscanf(data, "+QMTSTAT: %d,%d", &tcp_conn_id, &err_code);

    switch (err_code) {
        /* connection closed by server */
        case AT_QMTSTAT_CLOSED:

        /* send CONNECT package timeout or failure */
        case AT_QMTSTAT_CONNECT_TIMEOUT:

        /* recv CONNECK package timeout or failure */
        case AT_QMTSTAT_CONNACK_TIMEOUT:

        /* send package failure and disconnect by client */
        case AT_QMTSTAT_WORNG_CLOSE:
            /* ec800_rebuild_mqtt_network()不可以在这里直接执行，必须在另一个线程中执行  */
            rt_event_send(ec800.event, EVENT_URC_STATE_DISCONNECT);
            // ec800_rebuild_mqtt_network();
            break;

        /* send PINGREQ package timeout or failure */
        case AT_QMTSTAT_PINGREQ_TIMEOUT:
            ec800_deactivate_pdp();
            ec800_rebuild_mqtt_network();
            break;

        /* disconnect by client */
        case AT_QMTSTAT_DISCONNECT:
            NRF_LOG_DEBUG("disconnect by client");
            break;

        /* network inactivated or server unavailable */
        case AT_QMTSTAT_INACTIVATED:
            NRF_LOG_DEBUG("please check network");
            break;
        default:
            break;
    }
}

static void urc_mqtt_recv(struct at_client *client, const char *data, size_t size)
{
    /* 读取已从MQTT服务器接收的MQTT包数据 */
    NRF_LOG_DEBUG("AT client receive %d bytes data from server", size);
    NRF_LOG_DEBUG("%s", data);

    sscanf(data, "+QMTRECV: %*d,%*d,\"%*[^\"]\",%s", buf);
    ec800.parser(buf);
}

extern struct mqtt_data ec800_mqtt_data;
extern struct _time ec800_time;
static void urc_mqtt_gps(struct at_client *client, const char *data, size_t size)
{
    /* +QGPSGNMEA: $GNRMC,062848.00,A,2222.67783,N,11332.39237,E,0.000,,090923,,,A,V*16.. */
    const char *str    = data;
    char time[16]      = {0};
    char date[16]      = {0};
    char latitude[16]  = {0};
    char longitude[16] = {0};
    int i;
    if (strstr(data, "+QGPSLOC:")) {
        for (i = 0; i < 2; i++) {
            str = strstr(str, ",") + 1;
            if (*str == ',')
                continue;
            if (i == 0) {
                sscanf(str, "%[^,]", latitude);
                sscanf(latitude, "%lf", &ec800_mqtt_data.latitude);
            };
            if (i == 1) {
                sscanf(str, "%[^,]", longitude);
                sscanf(longitude, "%lf", &ec800_mqtt_data.longitude);
                rt_event_send(ec800.event, EVENT_GPS_LOC_UPDATED);
            };
        }
    }
#if GPS_TEST_ON
    if (strstr(data, "GSA")) {
        rt_device_t dev = rt_device_find("uart3");
        // memcpy(_buff_, data, strlen(data));
        if (dev)
            rt_device_write(dev, 0, data, strlen(data));
    }
#endif
    if (strstr(data, "$GNRMC")) {
        for (i = 0; i < 9; i++) {
            str = strstr(str, ",") + 1;
            if (*str == ',')
                continue;
            if (i == 0) {
                sscanf(str, "%[^.]", time);
                sscanf(time, "%2u%2u%2u", &ec800_time.hour, &ec800_time.minute, &ec800_time.second);
                ec800_time.hour += 8; // 北京时间+8h
                if (set_time(ec800_time.hour, ec800_time.minute, ec800_time.second) != EOK)
                    NRF_LOG_ERROR("set time fail.");
            }
            if (i == 8) {
                sscanf(str, "%[^,]", date);
                sscanf(date, "%2u%2u%2u", &ec800_time.day, &ec800_time.month, &ec800_time.year);
                ec800_time.year += 2000;
                if (set_date(ec800_time.year, ec800_time.month, ec800_time.day) != EOK)
                    NRF_LOG_ERROR("set date fail.");
                rt_event_send(ec800.event, EVENT_GPS_TIME_UPDATED);
            }
        }
    }
#if GPS_TEST_ON
    rt_event_send(ec800.event, EVENT_TEST_REPOINFO);
#endif
}

static void urc_report_state(struct at_client *client, const char *data, size_t size)
{
    LOG_I("Publish success.");
    rt_event_send(ec800.event, EVENT_MQTT_PUBLISH);
}

static void urc_csq_state(struct at_client *client, const char *data, size_t size)
{
#if GPS_TEST_ON
    extern int csq_rssi;
    extern int csq_ber;
    sscanf(data, "+CSQ: %d,%d", &csq_rssi, &csq_ber);
#endif
}

static const struct at_urc urc_table[] = {
    {"+QMTSTAT:", "\r\n", urc_mqtt_stat},
    {"+QMTRECV:", "\r\n", urc_mqtt_recv},
    {"+QGPSGNMEA:", "\r\n", urc_mqtt_gps},
    {"+QGPSLOC:", "\r\n", urc_mqtt_gps},
    {"+QMTPUBEX: 0,0,0", "\r\n", urc_report_state},
    {"+CSQ", "\r\n", urc_csq_state},
};

int at_client_port_init(void)
{
    /* 添加多种 URC 数据至 URC 列表中，当接收到同时匹配 URC 前缀和后缀的数据，执行 URC 函数  */
    at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    return EOK;
}

int is_moudle_power_on(void)
{
    /* 发送 AT 判断模块是否开机 */
    return check_send_cmd(AT_TEST, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int ec800_echo_off(void)
{
    return check_send_cmd(AT_ECHO_OFF, AT_OK, 1, AT_DEFAULT_TIMEOUT);
}

int ec800_open_gps(void)
{
    return check_send_cmd(EC800_AT_CMD_OPEN_GNSS, AT_OK, 1, AT_DEFAULT_TIMEOUT);
}

int ec800_gps_update(void)
{
    int result         = EOK;
    at_response_t resp = NULL;

    //    result = is_moudle_power_on();
    //    if (result != EOK)
    //        return result;

    // check_send_cmd(EC800_AT_CMD_OPEN_GNSS, AT_OK, 1, 2000);

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 1, rt_tick_from_millisecond(1000));
    if (resp == NULL) {
        NRF_LOG_ERROR("No memory for response structure!");
        return -ENOMEM;
    }

    ec800_open_gps();

#if GPS_TEST_ON
    resp->line_num = 2;
    result         = at_obj_exec_cmd(ec800.client, resp, "AT+QGPSGNMEA=\"GSA\"");
    if (result < 0) {
        NRF_LOG_ERROR("AT client send commands failed or wait response timeout!");
        goto __exit;
    }
#endif
    result = at_obj_exec_cmd(ec800.client, resp, EC800_AT_CMD_NEMA_RMC);

    result = at_obj_exec_cmd(ec800.client, resp, EC800_AT_CMD_GET_GPS_LOC);
    if (result < 0) {
        NRF_LOG_ERROR("AT client send commands failed or wait response timeout!");
        goto __exit;
    }
__exit:
    if (resp)
        at_delete_resp(resp);
    return result;
}

void ec800_subscribe_callback(const char *json)
{
    rt_kprintf("\r\nmessage:\r\n%s\r\n", json);
}

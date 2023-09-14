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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <at.h>

#define DBG_TAG "pkg.ec800_mqtt"
#ifdef PKG_USING_EC800_MQTT_DEBUG
#define DBG_LVL DBG_LOG
#else
#define DBG_LVL DBG_ERROR
#endif
#include <rtdbg.h>

#include "ec800_mqtt.h"

#define EC800_ADC0_PIN             -1
#define EC800_RESET_N_PIN          -1
#define EC800_OP_BAND              8

#define AT_CLIENT_DEV_NAME         "uart0"
#define AT_CLIENT_BAUD_RATE        115200

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

#define AT_CLIENT_RECV_BUFF_LEN    512
#define AT_DEFAULT_TIMEOUT         300

struct ec800_device ec800 = {
    .event     = NULL,
    .gps_state = 0,
    .reset_pin = EC800_RESET_N_PIN,
    .adc_pin   = EC800_ADC0_PIN,
    .stat      = EC800_STAT_DISCONNECTED};

static struct ec800_connect_obj ec800_connect = {
    .ip   = "129.226.207.116",
    .port = "1883"};

static char buf[AT_CLIENT_RECV_BUFF_LEN];

void ec800_subscribe_callback(const char *json);

/**
 * This function will show response information.
 *
 * @param resp  the response
 *
 * @return void
 */
static void show_resp_info(at_response_t resp)
{
    RT_ASSERT(resp);

#ifdef PKG_USING_EC800_MQTT_DEBUG
    /* Print response line buffer */
    const char *line_buffer = RT_NULL;

    for (rt_size_t line_num = 1; line_num <= resp->line_counts; line_num++) {
        if ((line_buffer = at_resp_get_line(resp, line_num)) != RT_NULL)
            LOG_I("line %d buffer : %s", line_num, line_buffer);
        else
            LOG_I("Parse line buffer error!");
    }
#endif
    return;
}

/**
 * This function will send command and check the result.
 *
 * @param client    at client handle
 * @param cmd       command to at client
 * @param resp_expr expected response expression
 * @param lines     response lines
 * @param timeout   waiting time
 *
 * @return  RT_EOK       send success
 *         -RT_ERROR     send failed
 *         -RT_ETIMEOUT  response timeout
 *         -RT_ENOMEM    alloc memory failed
 */
static int check_send_cmd(const char *cmd, const char *resp_expr,
                          const rt_size_t lines, const rt_int32_t timeout)
{
    at_response_t resp            = RT_NULL;
    int result                    = 0;
    char resp_arg[AT_CMD_MAX_LEN] = {0};

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, lines, rt_tick_from_millisecond(timeout));
    if (resp == RT_NULL) {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    result = at_obj_exec_cmd(ec800.client, resp, cmd);
    if (result < 0) {
        LOG_E("AT client send commands failed or wait response timeout!");
        goto __exit;
    }

    show_resp_info(resp);

    if (resp_expr) {
        if (at_resp_parse_line_args_by_kw(resp, resp_expr, "%s", resp_arg) <= 0) {
            LOG_D("# >_< Failed");
            result = -RT_ERROR;
            goto __exit;
        } else {
            LOG_D("# ^_^ successed");
        }
    }

    result = RT_EOK;

__exit:
    if (resp) {
        at_delete_resp(resp);
    }

    return result;
}

static char *ec800_get_imei(ec800_device_t device)
{
    at_response_t resp = RT_NULL;

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, rt_tick_from_millisecond(AT_DEFAULT_TIMEOUT));
    if (resp == RT_NULL) {
        LOG_E("No memory for response structure!");
        return RT_NULL;
    }

    /* send "AT+CGSN=1" commond to get device IMEI */
    if (at_obj_exec_cmd(device->client, resp, AT_QUERY_IMEI) != RT_EOK) {
        at_delete_resp(resp);
        return RT_NULL;
    }

    if (at_resp_parse_line_args(resp, 2, "+CGSN:%s", device->imei) <= 0) {
        LOG_E("device parse \"%s\" cmd error.", AT_QUERY_IMEI);
        at_delete_resp(resp);
        return RT_NULL;
    }
    LOG_D("IMEI code: %s", device->imei);

    at_delete_resp(resp);
    return device->imei;
}

static char *ec800_get_ipaddr(ec800_device_t device)
{
    at_response_t resp = RT_NULL;

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, rt_tick_from_millisecond(AT_DEFAULT_TIMEOUT));
    if (resp == RT_NULL) {
        LOG_E("No memory for response structure!");
        return RT_NULL;
    }

    /* send "AT+CGPADDR" commond to get IP address */
    if (at_obj_exec_cmd(device->client, resp, AT_QUERY_IPADDR) != RT_EOK) {
        at_delete_resp(resp);
        return RT_NULL;
    }

    /* parse response data "+CGPADDR: 0,<IP_address>" */
    if (at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR:%*d,%s", device->ipaddr) <= 0) {
        LOG_E("device parse \"%s\" cmd error.", AT_QUERY_IPADDR);
        at_delete_resp(resp);
        return RT_NULL;
    }
    LOG_D("IP address: %s", device->ipaddr);

    at_delete_resp(resp);
    return device->ipaddr;
}

static int ec800_mqtt_set_alive(rt_uint32_t keepalive_time)
{
    LOG_D("MQTT set alive.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_ALIVE, keepalive_time);

    return check_send_cmd(cmd, AT_OK, 0, 1000);
}

int ec800_mqtt_auth(void)
{
    LOG_D("MQTT set auth info.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_AUTH, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET);

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
    LOG_D("MQTT open socket.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    char *str                = NULL;
    char ip[16]              = {0};
    char port[8]             = {0};

    str = read_config_from_file("config.txt", "ip");
    str ? memcpy(ip, str, strlen(str)) : memcpy(ip, ec800_connect.ip, strlen(ec800_connect.ip));

    str = read_config_from_file("config.txt", "port");
    str ? memcpy(port, str, strlen(str)) : memcpy(port, ec800_connect.port, strlen(ec800_connect.port));

    rt_sprintf(cmd, AT_MQTT_OPEN, ip, port);

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
    LOG_D("MQTT close socket.");

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
    LOG_D("MQTT connect...");

    char cmd[AT_CMD_MAX_LEN] = {0};
    // rt_sprintf(cmd, AT_MQTT_CONNECT, ec800.imei);
    rt_sprintf(cmd, AT_MQTT_CONNECT, "1");
    LOG_D("%s", cmd);

    if (check_send_cmd(cmd, AT_MQTT_CONNECT_SUCC, 4, 10000) < 0) {
        LOG_D("MQTT connect failed.");
        return -RT_ERROR;
    }

    ec800.stat = EC800_STAT_CONNECTED;
    return RT_EOK;
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
        LOG_D("MQTT disconnect failed.");
        return -RT_ERROR;
    }

    ec800.stat = EC800_STAT_CONNECTED;
    return RT_EOK;
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
    rt_sprintf(cmd, AT_MQTT_SUB, topic);

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
    rt_sprintf(cmd, AT_MQTT_UNSUB, topic);

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
    // rt_sprintf(cmd, AT_MQTT_PUB, topic, strlen(msg));
    rt_sprintf(cmd, AT_MQTT_PUBEX, topic, strlen(msg));

    /* set AT client end sign to deal with '>' sign.*/
    at_set_end_sign('>');

    check_send_cmd(cmd, ">", 2, 1000);
    // check_send_cmd(cmd, ">", 0, AT_DEFAULT_TIMEOUT);
    LOG_D("publish...");

    /* reset the end sign for data conflict */
    // at_set_end_sign(0);

    return check_send_cmd(msg, AT_MQTT_PUB_SUCC, 4, 5000);
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

    while (RT_EOK != check_send_cmd("AT", AT_OK, 0, AT_DEFAULT_TIMEOUT)) {
        rt_thread_mdelay(1000);
        rt_kprintf("AT no responend\r\n");
    }

    rt_kprintf("AT responend OK\r\n");

    /* close echo */
    check_send_cmd(AT_ECHO_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);

    /* 关闭电信自动注册功能 */
    result = check_send_cmd(AT_QREGSWT_2, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 禁用自动连接网络 */
    result = check_send_cmd(AT_AUTOCONNECT_DISABLE, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 重启模块 */
    check_send_cmd(AT_REBOOT, AT_OK, 0, 10000);

    while (RT_EOK != check_send_cmd(AT_TEST, AT_OK, 0, AT_DEFAULT_TIMEOUT)) {
        rt_thread_mdelay(1000);
        rt_kprintf("AT no responend\r\n");
    }

    rt_kprintf("AT responend OK\r\n");

    /* 查询IMEI号 */
    result = check_send_cmd(AT_QUERY_IMEI, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    if (RT_NULL == ec800_get_imei(&ec800)) {
        LOG_E("Get IMEI code failed.");
        return -RT_ERROR;
    }

    /* 指定要搜索的频段 */
    rt_sprintf(cmd, AT_NBAND, EC800_OP_BAND);
    result = check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 打开模块的调试灯 */
    // result = check_send_cmd(AT_LED_ON, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    // if (result != RT_EOK) return result;

    /* 将模块设置为全功能模式(开启射频功能) */
    result = check_send_cmd(AT_FUN_ON, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 接收到TCP数据时，自动上报 */
    result = check_send_cmd(AT_RECV_AUTO, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 关闭eDRX */
    result = check_send_cmd(AT_EDRX_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 关闭PSM */
    result = check_send_cmd(AT_PSM_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 查询卡的国际识别码(IMSI号)，用于确认SIM卡插入正常 */
    result = check_send_cmd(AT_QUERY_IMSI, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 触发网络连接 */
    result = check_send_cmd(AT_UE_ATTACH, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 查询模块状态 */
    // at_exec_cmd(resp, "AT+NUESTATS");

    /* 查询网络注册状态 */
    // at_exec_cmd(resp, "AT+CEREG?");

    /* 查看信号强度 */
    // at_exec_cmd(resp, "AT+CSQ");

    /* 查询网络是否被激活，通常需要等待30s */
    int count = 60;
    while (count > 0 && RT_EOK != check_send_cmd(AT_QUERY_ATTACH, AT_UE_ATTACH_SUCC, 0, AT_DEFAULT_TIMEOUT)) {
        rt_thread_mdelay(1000);
        count--;
    }

    /* 查询模块的 IP 地址 */
    // at_exec_cmd(resp, "AT+CGPADDR");
    ec800_get_ipaddr(&ec800);

    if (count > 0) {
        ec800.stat = EC800_STAT_ATTACH;
        return RT_EOK;
    } else {
        return -RT_ETIMEOUT;
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
    if (RT_EOK != result) {
        return -RT_ERROR;
    }

    ec800.stat = EC800_STAT_DEATTACH;
    return RT_EOK;
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
    int result = RT_EOK;

    rt_device_t serial             = rt_device_find(AT_CLIENT_DEV_NAME);
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    config.baud_rate = AT_CLIENT_BAUD_RATE;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
#ifdef RT_USING_SERIAL_V2
    config.rx_bufsz = AT_CLIENT_RECV_BUFF_LEN;
#else
    config.bufsz = AT_CLIENT_RECV_BUFF_LEN;
#endif
    config.parity = PARITY_NONE;

    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);
    rt_device_close(serial);

    /* initialize AT client */
    result = at_client_init(AT_CLIENT_DEV_NAME, AT_CLIENT_RECV_BUFF_LEN);
    if (result < 0) {
        LOG_E("at client (%s) init failed.", AT_CLIENT_DEV_NAME);
        return result;
    }

    ec800.client = at_client_get(AT_CLIENT_DEV_NAME);
    if (ec800.client == RT_NULL) {
        LOG_E("get AT client (%s) failed.", AT_CLIENT_DEV_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}

void ec800_reset(void)
{
    rt_pin_mode(EC800_RESET_N_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(EC800_RESET_N_PIN, PIN_HIGH);

    rt_thread_mdelay(300);

    rt_pin_write(EC800_RESET_N_PIN, PIN_LOW);

    rt_thread_mdelay(1000);
}
MSH_CMD_EXPORT(ec800_reset, ec800_reset);

int at_client_port_init(void);
static void thread_mqtt_urc_handle(void *parameter);

/**
 * EC800 device initialize.
 *
 * @return 0 : initialize success
 *        -1 : initialize failed
 */
int ec800_init(void)
{
    LOG_D("Init at client device.");
    at_client_dev_init();
    at_client_port_init();

    LOG_D("Reset EC800 device.");
    // ec800_reset();

    ec800.stat = EC800_STAT_INIT;
    ec800_bind_parser(ec800_subscribe_callback);

    return RT_EOK;
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
        LOG_E("MQTT open failed.");
        return result;
    }

    if ((result = ec800_mqtt_connect()) < 0) {
        LOG_E("MQTT connect failed.");
        return result;
    }

    return RT_EOK;
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

    return RT_EOK;
}

static int ec800_deactivate_pdp(void)
{
    /* AT+CGACT=<state>,<cid> */

    return RT_EOK;
}

static void urc_mqtt_stat(struct at_client *client, const char *data, rt_size_t size)
{
    /* MQTT链路层的状态发生变化 */
    LOG_D("The state of the MQTT link layer changes");
    LOG_D("%s", data);

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
            LOG_D("disconnect by client");
            break;

        /* network inactivated or server unavailable */
        case AT_QMTSTAT_INACTIVATED:
            LOG_D("please check network");
            break;
        default:
            break;
    }
}

static void urc_mqtt_recv(struct at_client *client, const char *data, rt_size_t size)
{
    /* 读取已从MQTT服务器接收的MQTT包数据 */
    LOG_D("AT client receive %d bytes data from server", size);
    LOG_D("%s", data);

    sscanf(data, "+QMTRECV: %*d,%*d,\"%*[^\"]\",%s", buf);
    ec800.parser(buf);
}

extern struct mqtt_data ec800_mqtt_data;
extern struct _time ec800_time;
static void urc_mqtt_gps(struct at_client *client, const char *data, rt_size_t size)
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
            if (i == 0) {
                if (*str == ',')
                    continue;
                sscanf(str, "%[^,]", latitude);
                sscanf(latitude, "%lf", &ec800_mqtt_data.latitude);
            };
            if (i == 1) {
                if (*str == ',')
                    continue;
                sscanf(str, "%[^,]", longitude);
                sscanf(longitude, "%lf", &ec800_mqtt_data.longitude);
            };
        }

#if !GPS_TEST_ON
        // rt_event_send(ec800.event, EVENT_GPS_UPDATE_INFO);
#endif
    }
    if (strstr(data, "$GNRMC")) {
        for (i = 0; i < 9; i++) {
            str = strstr(str, ",") + 1;
            if (i == 0) {
                if (*str == ',')
                    continue;
                sscanf(str, "%[^.]", time);
                sscanf(time, "%2u%2u%2u", &ec800_time.hour, &ec800_time.minute, &ec800_time.second);
                ec800_time.hour += 8; // 北京时间+8h
                set_time(ec800_time.hour, ec800_time.minute, ec800_time.second);
            }
            if (i == 8) {
                if (*str == ',')
                    continue;
                sscanf(str, "%[^,]", date);
                sscanf(date, "%2u%2u%2u", &ec800_time.day, &ec800_time.month, &ec800_time.year);
                ec800_time.year += 2000;
                set_date(ec800_time.year, ec800_time.month, ec800_time.day);
            }
        }
#if !GPS_TEST_ON
        // rt_event_send(ec800.event, EVENT_GPS_UPDATE_INFO);
#endif
    }
#if !GPS_TEST_ON
    // rt_event_send(ec800.event, EVENT_GPS_UPDATE_INFO);
#endif
#if GPS_TEST_ON
    rt_event_send(ec800.event, EVENT_TEST_GPS_TIME);
#endif
}

static void urc_gps_state(struct at_client *client, const char *data, rt_size_t size)
{
    sscanf(data, "+QGPS: %d", &ec800.gps_state);
}

static const struct at_urc urc_table[] = {
    {"+QMTSTAT:", "\r\n", urc_mqtt_stat},
    {"+QMTRECV:", "\r\n", urc_mqtt_recv},
    {"+QGPSGNMEA:", "\r\n", urc_mqtt_gps},
    {"+QGPSLOC:", "\r\n", urc_mqtt_gps},
    {"+QGPS: ", "\r\n", urc_gps_state},
};

int at_client_port_init(void)
{
    /* 添加多种 URC 数据至 URC 列表中，当接收到同时匹配 URC 前缀和后缀的数据，执行 URC 函数  */
    at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    return RT_EOK;
}

#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT(ec800_mqtt_set_alive, AT client MQTT auth);
MSH_CMD_EXPORT(ec800_mqtt_auth, AT client MQTT auth);
MSH_CMD_EXPORT(ec800_mqtt_open, AT client MQTT open);
MSH_CMD_EXPORT(ec800_mqtt_close, AT client MQTT close);
MSH_CMD_EXPORT(ec800_mqtt_connect, AT client MQTT connect);
MSH_CMD_EXPORT(ec800_mqtt_disconnect, AT client MQTT disconnect);
MSH_CMD_EXPORT(ec800_mqtt_subscribe, AT client MQTT subscribe);
MSH_CMD_EXPORT(ec800_mqtt_unsubscribe, AT client MQTT unsubscribe);
MSH_CMD_EXPORT(ec800_mqtt_publish, AT client MQTT publish);

MSH_CMD_EXPORT(ec800_client_attach, AT client attach to access network);
MSH_CMD_EXPORT_ALIAS(at_client_dev_init, at_client_init, initialize AT client);
#endif

static void thread_mqtt_urc_handle(void *parameter)
{
    rt_uint32_t recv;
    while (1) {
        rt_event_recv(ec800.event, EVENT_URC_STATE_DISCONNECT,
                      RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &recv);
        switch (recv) {
            case EVENT_URC_STATE_DISCONNECT:
                ec800_rebuild_mqtt_network();
                break;
            default:
                break;
        }
    }
}

static int mqtt_urc_app_init(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("mqtt urc", thread_mqtt_urc_handle, NULL,
                           EC800_URC_THREAD_STACK_SIZE, EC800_URC_THREAD_PRIORITY, 200);
    if (tid) {
        rt_thread_startup(tid);
    } else {
        LOG_E("create mqtt app thread failed.");
        return -RT_ERROR;
    }
    return RT_EOK;
}
INIT_APP_EXPORT(mqtt_urc_app_init);

int is_moudle_power_on(void)
{
    /* 发送 AT 判断模块是否开机 */
    return check_send_cmd(AT_TEST, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int ec800_update_location(struct mqtt_data *ec800_mqtt_data)
{
    int result;

    //    result = is_moudle_power_on();
    //    if (result != RT_EOK) {
    //        ec800_power_switch();   //开机
    //        rt_thread_delay(5000);  //延迟5s再与模块通讯，否则模块可能不回复或者检测不到SIM卡
    //    }

    result = is_moudle_power_on();
    if (result != RT_EOK) {
        LOG_E("Error.");
        return result;
    }

    result = check_send_cmd(EC800_AT_CMD_OPEN_GNSS, AT_OK, 1, 3000);
    if (result != RT_EOK) {
        LOG_E("Error to exec %s.", EC800_AT_CMD_OPEN_GNSS);
        return result;
    }
    return 0;
}

int ec800_gps_update_info(void)
{
    int result         = RT_EOK;
    at_response_t resp = RT_NULL;

    result = is_moudle_power_on();
    if (result != RT_EOK)
        return result;

    // check_send_cmd(EC800_AT_CMD_OPEN_GNSS, AT_OK, 1, 2000);

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 1, rt_tick_from_millisecond(1000));
    if (resp == RT_NULL) {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    if (!ec800.gps_state) {
        //        at_client_obj_send(ec800.client, EC800_AT_CMD_GNSS_QUERY, strlen(EC800_AT_CMD_GNSS_QUERY));
        resp->line_num = 2;
        result         = at_obj_exec_cmd(ec800.client, resp, EC800_AT_CMD_GNSS_QUERY);
        if (result < 0) {
            LOG_E("AT client send commands failed or wait response timeout!");
            goto __exit;
        }
    }

    if (!ec800.gps_state) {
        //        at_client_obj_send(ec800.client, EC800_AT_CMD_OPEN_GNSS, strlen(EC800_AT_CMD_OPEN_GNSS));
        resp->line_num = 1;
        result         = at_obj_exec_cmd(ec800.client, resp, EC800_AT_CMD_OPEN_GNSS);
        if (result < 0) {
            LOG_E("AT client send commands failed or wait response timeout!");
            goto __exit;
        }
    }

    // at_client_obj_send(ec800.client, EC800_AT_CMD_NEMA_RMC, strlen(EC800_AT_CMD_NEMA_RMC));
    resp->line_num = 2;
    result         = at_obj_exec_cmd(ec800.client, resp, EC800_AT_CMD_NEMA_RMC);
    if (result < 0) {
        LOG_E("AT client send commands failed or wait response timeout!");
        goto __exit;
    }

    resp->line_num = 1;
    result         = at_obj_exec_cmd(ec800.client, resp, EC800_AT_CMD_GET_GPS_LOC);
    if (result < 0) {
        LOG_E("AT client send commands failed or wait response timeout!");
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
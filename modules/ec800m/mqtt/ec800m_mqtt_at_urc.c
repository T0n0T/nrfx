/**
 * @file ec800m_at_urc.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-28
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "ec800m.h"
#include "ec800m_mqtt.h"

#define NRF_LOG_MODULE_NAME ec800mqtturc
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

/** MQTOPEN */
#define URC_MQTOPEN_NETWORK_OPEN_FAIL -1
#define URC_MQTOPEN_OPEN_SUCCESS      0
#define URC_MQTOPEN_PARA_WRONG        1
#define URC_MQTOPEN_IDENTIFIER_WRONG  2
#define URC_MQTOPEN_PDP_WRONG         3
#define URC_MQTOPEN_DNS_WRONG         4
#define URC_MQTOPEN_NETWORK_DISC      5

/** MQTCONN */
#define URC_MQTCONN_MQTVER_WRONG            1
#define URC_MQTCONN_IDENTIFIER_WRONG        2
#define URC_MQTCONN_SERVER_WRONG            3
#define URC_MQTCONN_USER_SIGNIN_WRONG       4
#define URC_MQTCONN_CONNECT_REFUSED_WRONG   5

#define URC_MQTCONN_CHECK_CONNECT_INITIAL   1
#define URC_MQTCONN_CHECK_CONNECT_INPROCESS 2
#define URC_MQTCONN_CHECK_CONNECT_SUCCESS   3
#define URC_MQTCONN_CHECK_CONNECT_DISC      4

/** MQTSTAT */
#define URC_QMTSTAT_LINK_RESETED     1
#define URC_QMTSTAT_PINGREQ_TIMEOUT  2
#define URC_QMTSTAT_CONNECT_TIMEOUT  3
#define URC_QMTSTAT_CONNACK_TIMEOUT  4
#define URC_QMTSTAT_DISCONNECT       5
#define URC_QMTSTAT_TOO_MENY_WRONG   6
#define URC_QMTSTAT_LINK_INACTIVATED 7
#define URC_QMTSTAT_CLIENT_DISCONN   8

extern EventGroupHandle_t mqtt_event;
extern int                mqtt_task_publish(ec800m_mqtt_task_t task, uint32_t timeout);

void urc_mqtt_open(struct at_client* client, const char* data, size_t size)
{
    taskENTER_CRITICAL();
    int  res = 0, flag = 0;
    char res_str[40] = {0};
    if (sscanf(data, "+QMTOPEN: %*d,%d", &res) == 1) {
        if (res != 0) {
            switch (res) {
                case URC_MQTOPEN_NETWORK_OPEN_FAIL:
                    NRF_LOG_ERROR("network open fail.");
                    break;
                case URC_MQTOPEN_PARA_WRONG:
                    NRF_LOG_ERROR("parameter wrong.");
                    break;
                case URC_MQTOPEN_IDENTIFIER_WRONG:
                    NRF_LOG_ERROR("mqtt identifier has been used.");
                    break;
                case URC_MQTOPEN_PDP_WRONG:
                    NRF_LOG_ERROR("PDP activate failed.");
                    break;
                case URC_MQTOPEN_DNS_WRONG:
                    NRF_LOG_ERROR("DNS failed.");
                    break;
                case URC_MQTOPEN_NETWORK_DISC:
                    NRF_LOG_ERROR("network disconnected.");
                    break;
                default:
                    break;
            }
        } else {
            NRF_LOG_DEBUG("mqtt release success.");
            flag = 1;
        }
    } else if (sscanf(data, "+QMTOPEN: %*d,%s", res_str) > 0) {
        NRF_LOG_DEBUG("mqtt has done release of [%s]", res_str);
        flag = 1;
    } else {
        mqtt_status = EC800M_IDLE;
    }
    taskEXIT_CRITICAL();
    if (flag == 1) {
        mqtt_status = EC800M_MQTT_OPEN;
        mqtt_task_publish(EC800M_TASK_MQTT_CONNECT, 5000);
    }
}

void urc_mqtt_close(struct at_client* client, const char* data, size_t size)
{
    if (mqtt_status == EC800M_MQTT_IDLE) {
        taskENTER_CRITICAL();
        mqtt_status = EC800M_MQTT_CLOSE;
        taskEXIT_CRITICAL();
        mqtt_task_publish(EC800M_TASK_MQTT_OPEN, 0);
    }
}

void urc_mqtt_conn(struct at_client* client, const char* data, size_t size)
{
    xTimerStop(ec800m.timer, 0);
    taskENTER_CRITICAL();
    int         res = 0, ret_code = 0, state = 0;
    EventBits_t event = 0;
    if (sscanf(data, "+QMTCONN: %*d,%d,%d", &res, &ret_code) == 2) {
        if (!ret_code && !res) {
            NRF_LOG_INFO("mqtt connect success.");
            mqtt_status = EC800M_MQTT_CONN;
            event       = EC800M_MQTT_SUCCESS;
        } else {
            event = EC800M_MQTT_ERROR;
            if (ret_code != 0) {
                switch (ret_code) {
                    case URC_MQTCONN_MQTVER_WRONG:
                        NRF_LOG_ERROR("mqtt version wrong.");
                        break;
                    case URC_MQTCONN_IDENTIFIER_WRONG:
                        NRF_LOG_ERROR("mqtt identifier wrong.");
                        break;
                    case URC_MQTCONN_SERVER_WRONG:
                        NRF_LOG_ERROR("mqtt server wrong.");
                        break;
                    case URC_MQTCONN_USER_SIGNIN_WRONG:
                        NRF_LOG_ERROR("mqtt username or passwd wrong.");
                        break;
                    case URC_MQTCONN_CONNECT_REFUSED_WRONG:
                        NRF_LOG_ERROR("mqtt connect refused.");
                        break;
                    default:
                        break;
                }
            }
            if (res != 0) {
                NRF_LOG_ERROR("ec800m mqtt connect failed.");
                switch (res) {
                    case 1:
                        NRF_LOG_WARNING("mqtt connack wait for retransmission.");
                        break;
                    case 2:
                        NRF_LOG_WARNING("mqtt connack transmit fail.");
                    default:
                        break;
                }
            }
        }
    } else if (sscanf(data, "+QMTCONN: %*d,%d", &state) == 1) {
        switch (state) {
            case URC_MQTCONN_CHECK_CONNECT_INITIAL:
                NRF_LOG_INFO("mqtt is initializing.");
                break;
            case URC_MQTCONN_CHECK_CONNECT_INPROCESS:
                NRF_LOG_INFO("mqtt is connecting.");
                break;
            case URC_MQTCONN_CHECK_CONNECT_SUCCESS:
                NRF_LOG_INFO("mqtt connect success.");
                event = EC800M_MQTT_SUCCESS;
                break;
            case URC_MQTCONN_CHECK_CONNECT_DISC:
                NRF_LOG_INFO("mqtt is disconnecting.");
                event = EC800M_MQTT_ERROR;
                break;
            default:
                event = EC800M_MQTT_ERROR;
                break;
        }
    }
    taskEXIT_CRITICAL();
    xEventGroupSetBits(mqtt_event, event);
}

void urc_mqtt_stat(struct at_client* client, const char* data, size_t size)
{
    taskENTER_CRITICAL();
    int err_code = 0, task = 0;
    sscanf(data, "+QMTSTAT: %*d,%d", &err_code);
    switch (err_code) {
        case URC_QMTSTAT_LINK_RESETED:
            vTaskDelay(500);
            task = EC800M_TASK_MQTT_OPEN;
            break;
        case URC_QMTSTAT_PINGREQ_TIMEOUT:
            break;
        case URC_QMTSTAT_CONNECT_TIMEOUT:
        case URC_QMTSTAT_CONNACK_TIMEOUT:
            NRF_LOG_ERROR("timeout happened from user sigin");
            break;
        case URC_QMTSTAT_DISCONNECT:
            NRF_LOG_DEBUG("mqtt is disconnected.");
            vTaskDelay(500);
            mqtt_status = EC800M_IDLE;
            task        = EC800M_TASK_MQTT_CLOSE;
            break;
        case URC_QMTSTAT_TOO_MENY_WRONG:
            NRF_LOG_DEBUG("mqtt is disconnected.");
            vTaskDelay(500);
            mqtt_status = EC800M_IDLE;
            task        = EC800M_TASK_MQTT_CLOSE;
            break;
        case URC_QMTSTAT_LINK_INACTIVATED:
            NRF_LOG_ERROR("timeout happened from dns or other network problems");
            break;
        case URC_QMTSTAT_CLIENT_DISCONN:
            break;
        default:
            break;
    }
    taskEXIT_CRITICAL();
    if (task) {
        mqtt_task_publish(EC800M_TASK_MQTT_OPEN, 0);
    }
}

void urc_mqtt_recv(struct at_client* client, const char* data, size_t size)
{
    taskENTER_CRITICAL();
    static char recv_buf[EC800M_BUF_LEN];
    sscanf(data, "+QMTRECV: %*d,%*d,\"%*[^\"]\",%s", recv_buf);
    NRF_LOG_DEBUG("recv: %s", recv_buf);
    taskEXIT_CRITICAL();
}

const struct at_urc mqtt_urc_table[] = {
    {"+QMTOPEN:", "\r\n", urc_mqtt_open},
    {"+QMTCLOSE:", "\r\n", urc_mqtt_close},
    {"+QMTCONN:", "\r\n", urc_mqtt_conn},
    {"+QMTSTAT:", "\r\n", urc_mqtt_stat},
    {"+QMTRECV:", "\r\n", urc_mqtt_recv},
};
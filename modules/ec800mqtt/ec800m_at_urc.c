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

#define NRF_LOG_MODULE_NAME ec800urc
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

void urc_mqtt_open(struct at_client* client, const char* data, size_t size)
{
    taskENTER_CRITICAL();
    int  res;
    char res_str[40] = {0};
    if (sscanf(data, "+QMTOPEN: %*d,%d", &res) == 1) {
        if (res != 0) {
            switch (res) {
                case -1:
                    NRF_LOG_ERROR("network open fail.");
                    break;
                case 1:
                    NRF_LOG_ERROR("parameter wrong.");
                    break;
                case 2:
                    NRF_LOG_ERROR("mqtt identifier has been used.");
                    break;
                case 3:
                    NRF_LOG_ERROR("PDP activate failed.");
                    break;
                case 4:
                    NRF_LOG_ERROR("DNS failed.");
                    break;
                case 6:
                    NRF_LOG_ERROR("network disconnected.");
                    break;
                default:
                    break;
            }
        } else {
            NRF_LOG_DEBUG("mqtt release success.");
            ec800m.status = EC800M_MQTT_OPEN;
            xEventGroupSetBits(ec800m.event, EC800M_EVENT_MQTT_CONNECT);
        }

    } else if (sscanf(data, "+QMTOPEN: %*d,%s", res_str) > 0) {
        NRF_LOG_DEBUG("mqtt has done release of [%s]", res_str);
        ec800m.status = EC800M_MQTT_OPEN;
        xEventGroupSetBits(ec800m.event, EC800M_EVENT_MQTT_CONNECT);
    } else {
        ec800m.status = EC800M_IDLE;
    }
    taskEXIT_CRITICAL();
}

void urc_mqtt_close(struct at_client* client, const char* data, size_t size)
{
    taskENTER_CRITICAL();
    ec800m.status = EC800M_MQTT_CLOSE;
    taskEXIT_CRITICAL();
}

void urc_mqtt_conn(struct at_client* client, const char* data, size_t size)
{
    int res = 0, ret_code = 0, state = 0;
    if (sscanf(data, "+QMTCONN: %*d,%d,%d", &res, &ret_code) == 2) {
        if (!ret_code && !res) {
            NRF_LOG_INFO("mqtt connect success.");

        } else {
            if (ret_code != 0) {
                switch (ret_code) {
                    case 1:
                        NRF_LOG_ERROR("mqtt version wrong.");
                        break;
                    case 2:
                        NRF_LOG_ERROR("mqtt identifier wrong.");
                        break;
                    case 3:
                        NRF_LOG_ERROR("mqtt server wrong.");
                        break;
                    case 4:
                        NRF_LOG_ERROR("mqtt username or passwd wrong.");
                        break;
                    case 5:
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
            case 1:
                NRF_LOG_INFO("mqtt is initializing.");
                break;
            case 2:
                NRF_LOG_INFO("mqtt is connecting.");
                break;
            case 3:
                NRF_LOG_INFO("mqtt connect success.");
                ec800m.status = EC800M_MQTT_CONN;
                break;
            case 4:
                NRF_LOG_INFO("mqtt is disconnecting.");
                ec800m.status = EC800M_MQTT_DISC;
                break;
            default:
                break;
        }
    }
}

void urc_mqtt_stat(struct at_client* client, const char* data, size_t size)
{
}

void urc_mqtt_recv(struct at_client* client, const char* data, size_t size)
{
}

void urc_report_state(struct at_client* client, const char* data, size_t size)
{
}

const struct at_urc urc_table[] = {
    {"+QMTOPEN:", "\r\n", urc_mqtt_open},
    {"+QMTCLOSE:", "\r\n", urc_mqtt_close},
    {"+QMTCONN:", "\r\n", urc_mqtt_conn},
    {"+QMTSTAT:", "\r\n", urc_mqtt_stat},
    {"+QMTRECV:", "\r\n", urc_mqtt_recv},
    {"+QMTPUBEX: 0,0,0", "\r\n", urc_report_state},
};
/**
 * @file ec800m_at_cmd.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-10-27
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "ec800m.h"

const struct at_cmd at_cmd_list[] = {
    /** common */
    {AT_CMD_NAME(AT_CMD_TEST), "AT", "OK", 300},
    {AT_CMD_NAME(AT_CMD_RESET), "AT&F", "OK", 300},
    {AT_CMD_NAME(AT_CMD_ECHO_OFF), "ATE0", "OK", 300},
    {AT_CMD_NAME(AT_CMD_LOW_POWER), "AT+QSCLK=1", "OK", 300},

    /** simcard */
    {AT_CMD_NAME(AT_CMD_IMSI), "AT+CIMI", "OK", 300},
    {AT_CMD_NAME(AT_CMD_ICCID), "AT+QCCID", "+QCCID:", 300},
    {AT_CMD_NAME(AT_CMD_GSN), "AT+GSN", "OK", 300},
    {AT_CMD_NAME(AT_CMD_CHECK_SIGNAL), "AT+CSQ", "+CSQ:", 300},
    {AT_CMD_NAME(AT_CMD_CHECK_GPRS), "AT+CGREG?", "+CGREG:", 300},
    {AT_CMD_NAME(AT_CMD_CHECK_PIN), "AT+CPIN?", "READY", 5000},

    /** tcp/ip using contextID=1 */
    {AT_CMD_NAME(AT_CMD_ACT_PDP), "AT+QIACT=1", "OK", 1500000},
    {AT_CMD_NAME(AT_CMD_DEACT_PDP), "AT+QIDEACT=1", "OK", 40000},
    {AT_CMD_NAME(AT_CMD_DATAECHO_OFF), "AT+QISDE=0", "OK", 300},
    {AT_CMD_NAME(AT_CMD_PING), "AT+QPING=1,%s,%d,1", "+QPING:", 5000},
    {AT_CMD_NAME(AT_CMD_QUERY_TARGET_IP), "AT+QIDNSGIP=1,\"%s\"", "OK", 60000},
    {AT_CMD_NAME(AT_CMD_QUERY_OWN_IP), "AT+CGPADDR=1", "+CGPADDR:", 300},
    {AT_CMD_NAME(AT_CMD_CONF_DNS), "AT+QIDNSCFG=1,%s", "OK", 300},
    {AT_CMD_NAME(AT_CMD_QUERY_DNS), "AT+QIDNSCFG=1", "+QIDNSCFG:", 300},

    /** socket,direct-out mode */
    {AT_CMD_NAME(AT_CMD_SOCKET_OPEN), "AT+QIOPEN=1,%d,\"%s\",\"%s\",\"%s\",0,1", "OK", 1500000},
    {AT_CMD_NAME(AT_CMD_SOCKET_CLOSE), "AT+QICLOSE=%d", "OK", 10000},
    {AT_CMD_NAME(AT_CMD_SOCKET_SEND), "AT+QISEND=%d,%d", "OK", 300},
    {AT_CMD_NAME(AT_CMD_SOCKET_RECV), "AT+QIRD=%d,%d", "OK", 300},
    {AT_CMD_NAME(AT_CMD_SOCKET_STATUS), "AT+QISTATE?", "OK", 5000},

    /** gnss */
    {AT_CMD_NAME(AT_CMD_GNSS_OPEN), "AT+QGPS=1", "OK", 300},
    {AT_CMD_NAME(AT_CMD_GNSS_CLOSE), "AT+QGPS=0", "OK", 300},
    {AT_CMD_NAME(AT_CMD_GNSS_STATUS), "AT+QGPS?", "+QGPS:", 300},
    {AT_CMD_NAME(AT_CMD_GNSS_LOC), "AT+QGPSLOC=2", "+QGPSLOC:", 300},
    {AT_CMD_NAME(AT_CMD_GNSS_NMEA_CONF), "AT+QGPSCFG=\"gpsnmeatype\",%d", "OK", 300},
    {AT_CMD_NAME(AT_CMD_GNSS_NMEA_RMC), "AT+QGPSGNMEA=\"RMC\"", "+QGPSGNMEA:", 300},

    /** mqtt using client_idx=0 */
    {AT_CMD_NAME(AT_CMD_MQTT_REL), "AT+QMTOPEN=0,\"%s\",%s", "OK", 3000},
    {AT_CMD_NAME(AT_CMD_MQTT_CHECK_REL), "AT+QMTOPEN?", "OK", 300},
    {AT_CMD_NAME(AT_CMD_MQTT_CLOSE), "AT+QMTCLOSE=0", "OK", 3000},
    {AT_CMD_NAME(AT_CMD_MQTT_STATUS), "AT+QMTCONN?", "OK", 5000},
    {AT_CMD_NAME(AT_CMD_MQTT_CONNECT), "AT+QMTCONN=0,\"%s\"", "OK", 5000},
    {AT_CMD_NAME(AT_CMD_MQTT_DISCONNECT), "AT+QMTDISC=0", "OK", 30000},
    {AT_CMD_NAME(AT_CMD_MQTT_PUBLISH), "AT+QMTPUBEX=0,0,0,0,\"%s\",%d", "", 3000},
    {AT_CMD_NAME(AT_CMD_MQTT_SUBSCRIBE), "AT+QMTSUB=0,1,\"%s\",2", "OK", 3000},
    {AT_CMD_NAME(AT_CMD_MQTT_READBUF), "AT+QMTRECV=0", "OK", 5000},
    {AT_CMD_NAME(AT_CMD_MQTT_CONF_ALIVE), "AT+QMTCFG=\"keepalive\",0,%d", "OK", 300},
};

/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-04-20     qiyongzhong       first version
 */

#ifndef __AT_DEVICE_EC800X_H__
#define __AT_DEVICE_EC800X_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "gps_rmc.h"
#include <at_device.h>

#define AT_DEVICE_CLASS_EC800X 0X17U
/* The maximum number of sockets supported by the ec800x device */
#define AT_DEVICE_EC800X_SOCKETS_NUM 5

#define AT_DEVICE_USING_EC800X
#define AT_DEVICE_EC800X_SOCKET
#define AT_DEVICE_EC800X_SAMPLE
#define AT_DEVICE_EC800X_INIT_ASYN
#define EC800X_SAMPLE_POWER_PIN     -1
#define EC800X_SAMPLE_STATUS_PIN    -1
#define EC800X_SAMPLE_WAKEUP_PIN    -1
#define EC800X_SAMPLE_CLIENT_NAME   "uart0"
#define EC800X_SAMPLE_RECV_BUFF_LEN 512

#define EC800X_GPS_MSG_NUM          128

struct at_device_ec800x {
    char *device_name;
    char *client_name;

    int power_pin;
    int power_status_pin;
    int wakeup_pin;
    size_t recv_line_num;
    void (*power_ctrl)(int is_on);
    struct at_device device;

    void *socket_data;
    void *user_data;

    rt_bool_t power_status;
    rt_bool_t sleep_status;
    int rssi;

    char rmc[EC800X_GPS_MSG_NUM];
    gps_info_t rmc_info;
};

#ifdef AT_USING_SOCKET

/* ec800x device socket initialize */
int ec800x_socket_init(struct at_device *device);

/* ec800x device class socket register */
int ec800x_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_EC800X_H__ */

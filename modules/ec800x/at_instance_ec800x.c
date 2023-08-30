/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2019-12-13     qiyongzhong       first version
 */

#include <at_device_ec800x.h>

#define LOG_TAG "at.sample.ec800x"
#include <at_log.h>

#define EC800X_SAMPLE_DEIVCE_NAME "ec800x"

static struct at_device_ec800x _dev =
    {
        EC800X_SAMPLE_DEIVCE_NAME,
        EC800X_SAMPLE_CLIENT_NAME,

        EC800X_SAMPLE_POWER_PIN,
        EC800X_SAMPLE_RESET_PIN,
        EC800X_SAMPLE_WAKEUP_PIN,
        EC800X_SAMPLE_RECV_BUFF_LEN,
        RT_NULL};

static int ec800x_device_register(void)
{
    struct at_device_ec800x *ec800x = &_dev;

    return at_device_register(&(ec800x->device),
                              ec800x->device_name,
                              ec800x->client_name,
                              AT_DEVICE_CLASS_EC800X,
                              (void *)ec800x);
}
INIT_APP_EXPORT(ec800x_device_register);

int ec800x_get_rssi(void)
{
    struct at_device_ec800x *ec800x = &_dev;
    return (ec800x->rssi);
}

#if defined(USING_RMC)
struct gps_info *ec800x_get_gnss(void)
{
    return &rmcinfo;
}
#elif defined(USING_LOC)
struct LOC_GNSS *ec800x_get_gnss(void)
{
    return &gnssmsg;
}
#endif

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

struct at_device_ec800x _dev =
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
// INIT_APP_EXPORT(ec800x_device_register);

rt_bool_t ec800x_isinit(void)
{
    struct at_device_ec800x *ec800x = &_dev;
    return (ec800x->device.is_init);
}

void ec800x_status(void)
{
    struct at_device_ec800x *ec800x = &_dev;
    printf("status of ec800 is %d\n", ec800x_isinit());
    printf("status of gnss is %d\n", ec800x->gnss_status);
}
MSH_CMD_EXPORT(ec800x_status, ec800x status);

int ec800x_get_rssi(void)
{
    struct at_device_ec800x *ec800x = &_dev;
    return (ec800x->rssi);
}

#if defined(USING_RMC)
gps_info_t ec800x_get_gnss(void)
{
    return &rmcinfo;
}
#elif defined(USING_LOC)
struct LOC_GNSS *ec800x_get_gnss(void)
{
    return &gnssmsg;
}
#endif

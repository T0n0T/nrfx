/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-15     xckhmf       First Verison
 *
 */
#ifndef __DRV_I2C_H__
#define __DRV_I2C_H__

#include <nrfx_twim.h>
#include <nrfx_twi_twim.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    nrf_twim_frequency_t freq;
    uint32_t scl_pin;
    uint32_t sda_pin;
    nrfx_twim_t twi_instance;
    uint8_t transfer_status;
    uint8_t need_recover;
} drv_i2c_cfg_t;

void i2c0_init(void);
void i2c0_uninit(void);

enum twim_status {
    I2C_RECOVER = 1,
    I2C_DISABLE
};

#ifdef __cplusplus
}
#endif

#endif /* __DRV_I2C_H__ */

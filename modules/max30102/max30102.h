/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-02-08     Jackistang   the first version
 */
#ifndef PKG_MAX30102_H__
#define PKG_MAX30102_H__

#include "nrf_gpio.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#ifndef MAX30102_STACK_SIZE
#define MAX30102_STACK_SIZE 0x300
#endif

#ifndef MAX30102_PRIORITY
#define MAX30102_PRIORITY 10
#endif

#ifndef MAX30102_TICKS
#define MAX30102_TICKS 10
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) \
    ((unsigned long)(sizeof(array) / sizeof((array)[0])))
#endif

// register addresses
#define REG_INTR_STATUS_1   0x00
#define REG_INTR_STATUS_2   0x01
#define REG_INTR_ENABLE_1   0x02
#define REG_INTR_ENABLE_2   0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C
#define REG_LED2_PA         0x0D
#define REG_PILOT_PA        0x10
#define REG_MULTI_LED_CTRL1 0x11
#define REG_MULTI_LED_CTRL2 0x12
#define REG_TEMP_INTR       0x1F
#define REG_TEMP_FRAC       0x20
#define REG_TEMP_CONFIG     0x21
#define REG_PROX_INT_THRESH 0x30
#define REG_REV_ID          0xFE
#define REG_PART_ID         0xFF

#define MAX30102_ADDR       0x57 // 7-bit version of the above

extern void max30102_thread_entry(void* args);

bool maxim_max30102_init_proximity_mode();
bool maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data);
bool maxim_max30102_read_reg(uint8_t uch_addr, uint8_t* data, uint16_t len);

void max30102_init(void);
void twim_init(void);
void twim_uninit(void);
#endif /*  MAX30102_H_ */

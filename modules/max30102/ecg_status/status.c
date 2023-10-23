/**
 * @file status.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "blood.h"
#include "max30102.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>

#define DBG_LEVEL DBG_LOG
#define LOG_TAG   "ecg.status"
#include <rtdbg.h>

#define

HRM_Mode current_mode;
int checkout_spo2_flag, checkout_prox_flag;
BloodData g_blooddata = {0}; // 血液数据存储

uint32_t ir_buffer[500];  // IR LED sensor data
uint32_t red_buffer[500]; // Red LED sensor data

static void max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
    uint16_t un_temp;
    uint8_t uch_temp;
    uint8_t ach_i2c_data[6] = {0};

    if (!maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_temp, sizeof(uch_temp))) {
        LOG_E("Max30102 clean intr1 failed.");
        return;
    }
    if (!maxim_max30102_read_reg(REG_INTR_STATUS_2, &uch_temp, sizeof(uch_temp))) {
        LOG_E("Max30102 clean intr2 failed.");
        return;
    }

    if (!maxim_max30102_read_reg(REG_FIFO_DATA, ach_i2c_data, ARRAY_SIZE(ach_i2c_data))) {
        LOG_E("Max30102 read fifo failed.");
        return;
    }

    if (pun_red_led != 0) {
        *pun_red_led = 0;
        un_temp      = ach_i2c_data[0];
        un_temp <<= 14;
        *pun_red_led += un_temp;
        un_temp = ach_i2c_data[1];
        un_temp <<= 6;
        *pun_red_led += un_temp;
        un_temp = ach_i2c_data[2];
        un_temp >>= 2;
        *pun_red_led += un_temp;
    }

    if (pun_ir_led != 0) {
        *pun_ir_led = 0;
        un_temp     = ach_i2c_data[3];
        un_temp <<= 14;
        *pun_ir_led += un_temp;
        un_temp = ach_i2c_data[4];
        un_temp <<= 6;
        *pun_ir_led += un_temp;
        un_temp = ach_i2c_data[5];
        un_temp >>= 2;
        *pun_ir_led += un_temp;
    }
}

void blood_data_update_proximity(void)
{
    // 模式切换
    int retry         = 8;
    uint8_t reg       = 0;
    uint8_t read_ptr  = 0;
    uint8_t write_ptr = 0;
    uint32_t fifo_ir;
    while (rt_pin_read(cfg.irq_pin.pin) == 0) {
        while (retry--) {
            max30102_read_fifo(0, &fifo_ir); // read from MAX30102 FIFO2
            // LOG_D("mode get data[%d]:  fifo_ir: %06d fifo_red: %06d", 8 - retry, fifo_ir, fifo_red);
            if (fifo_ir > 10000 && current_mode == PROX) {
                if (checkout_spo2_flag > 3) {
                    checkout_spo2_flag = 0;
                    max30102_checkout_HRM_SPO2_mode();
                    current_mode = HRM_SPO2;
                    LOG_D("checkout into hrm_spo2.");
                    goto MODE_ENTRY;
                }
                checkout_prox_flag = 0;
                checkout_spo2_flag++;
                continue;
            } else if (fifo_ir < 5000 && current_mode == HRM_SPO2) {
                if (checkout_prox_flag > 3) {
                    checkout_prox_flag = 0;
                    max30102_checkout_proximity_mode();
                    current_mode = PROX;
                    LOG_D("checkout into prox_mode.");
                    goto MODE_ENTRY;
                }
                checkout_spo2_flag = 0;
                checkout_prox_flag++;
                continue;
            }
        }
        break;
    }

MODE_ENTRY:
    // 数据写入
    if (current_mode == HRM_SPO2) {
        LOG_D("HRM_SPO2_MODE!!!!!!!!!!!!!!!!!!!");
    }
}

void max30102_ecg_status_get(void)
{
    uint32_t un_min, un_max;
    uint32_t un_prev_data;
    int i;

    un_min = 0x3FFFF;
    un_max = 0;

    for (i = 0; i < 500; i++) {
        while (rt_pin_read(cfg.irq_pin.pin) == 1)
            ; // wait until the interrupt pin asserts
        max30102_read_fifo(&red_buffer[i], &ir_buffer[i]);

        if (un_min > red_buffer[i])
            un_min = red_buffer[i]; // update signal min
        if (un_max < red_buffer[i])
            un_max = red_buffer[i]; // update signal max
    }
}

void blood_Loop(void)
{
    // 血液信息获取
    blood_data_update_proximity();

    if (current_mode == HRM_SPO2) {
    }
}

void max30102_thread_entry(void *args)
{
    checkout_spo2_flag = 0;
    checkout_prox_flag = 0;
    current_mode       = PROX;

    int32_t n_heart_rate = 0;
    int32_t n_sp02       = 0;
    rt_pin_mode(cfg.irq_pin.pin, PIN_MODE_INPUT_PULLUP);

    while (1) {
        blood_Loop();
        rt_thread_mdelay(500);
    }
}
#include "blood.h"
#include "max30102.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>

#define DBG_LEVEL DBG_LOG
#define LOG_TAG   "blood.cal"
#include <rtdbg.h>
/**
  ******************************************************************************
  * 使用实例:
             int32_t n_heart_rate = 0;
             int32_t n_sp02 = 0;

             max30102_ecg_init();

             while(1)
             {
                 max30102_data_handle(&n_heart_rate, &n_sp02);            //
                 printf("heart:%d, sp02:%d\r\n", n_heart_rate, n_sp02);
             }

  *
  ******************************************************************************
  */

#define MAX_BRIGHTNESS 255

uint32_t aun_ir_buffer[500];  // IR LED sensor data
int32_t n_ir_buffer_length;   // data length
uint32_t aun_red_buffer[500]; // Red LED sensor data
int32_t n_sp02;               // SPO2 value
int8_t ch_spo2_valid;         // indicator to show if the SP02 calculation is valid
int32_t n_heart_rate;         // heart rate value
int8_t ch_hr_valid;           // indicator to show if the heart rate calculation is valid
uint8_t uch_dummy;
int heart = 0;

void max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led);

void max30102_ecg_init(void)
{
    uint32_t un_min, un_max;
    uint32_t un_prev_data;
    int i;
    uint8_t temp[6];

    un_min             = 0x3FFFF;
    un_max             = 0;
    n_ir_buffer_length = 500; // buffer length of 100 stores 5 seconds of samples running at 100sps

    for (i = 0; i < n_ir_buffer_length; i++) {
        while (rt_pin_read(cfg.irq_pin.pin) == 1)
            ; // wait until the interrupt pin asserts
        max30102_read_fifo(&aun_red_buffer[i], &aun_ir_buffer[i]);

        if (un_min > aun_red_buffer[i])
            un_min = aun_red_buffer[i]; // update signal min
        if (un_max < aun_red_buffer[i])
            un_max = aun_red_buffer[i]; // update signal max
    }
    un_prev_data = aun_red_buffer[i];
    un_prev_data = un_prev_data;
    maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, aun_red_buffer, &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
}

void max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
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
        un_temp = ach_i2c_data[0];
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
        un_temp = ach_i2c_data[3];
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

void max30102_data_handle(int32_t *heart_rate, int32_t *sp02)
{
    // variables to calculate the on-board LED brightness that reflects the heartbeats
    uint32_t un_min, un_max, un_prev_data;
    int i;
    uint8_t temp[6];
    float f_temp;
    int32_t n_brightness;

    while (1) {
        i      = 0;
        un_min = 0x3FFFF;
        un_max = 0;
        for (i = 100; i < 500; i++) {
            aun_red_buffer[i - 100] = aun_red_buffer[i];
            aun_ir_buffer[i - 100]  = aun_ir_buffer[i];

            // update the signal min and max
            if (un_min > aun_red_buffer[i])
                un_min = aun_red_buffer[i];
            if (un_max < aun_red_buffer[i])
                un_max = aun_red_buffer[i];
        }
        // take 100 sets of samples before calculating the heart rate.
        for (i = 400; i < 500; i++) {
            un_prev_data = aun_red_buffer[i - 1];
            while (rt_pin_read(cfg.irq_pin.pin) != 0) {
                rt_thread_mdelay(1);
                printf("waiting for irq pin low\n");
            }

            max30102_read_fifo(&aun_red_buffer[i], &aun_ir_buffer[i]);

            if (aun_red_buffer[i] > un_prev_data) {
                f_temp = aun_red_buffer[i] - un_prev_data;
                f_temp /= (un_max - un_min);
                f_temp *= MAX_BRIGHTNESS;
                n_brightness -= (int)f_temp;
                if (n_brightness < 0)
                    n_brightness = 0;
            } else {
                f_temp = un_prev_data - aun_red_buffer[i];
                f_temp /= (un_max - un_min);
                f_temp *= MAX_BRIGHTNESS;
                n_brightness += (int)f_temp;
                if (n_brightness > MAX_BRIGHTNESS)
                    n_brightness = MAX_BRIGHTNESS;
            }
            // send samples and calculation result to terminal program through UART
            if (ch_hr_valid == 1 && ch_spo2_valid == 1 && n_heart_rate < 120 && n_sp02 < 101) //**/ ch_hr_valid == 1 && ch_spo2_valid ==1 && n_heart_rate<120 && n_sp02<101
            {
                heart = n_heart_rate;
            } else {
                n_heart_rate = 0;
                heart        = 0;
            }
            if (ch_hr_valid == 0) {
                n_heart_rate = 0;
                heart        = 0;
            }
        }

        maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, aun_red_buffer, &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
        if ((ch_hr_valid == 1) && (ch_spo2_valid == 1) && (n_heart_rate > 50) && (n_heart_rate < 120) && (n_sp02 < 101) && (n_sp02 > 80)) //**/ ch_hr_valid == 1 && ch_spo2_valid ==1 && n_heart_rate<120 && n_sp02<101
        {
            *heart_rate = n_heart_rate;
            *sp02       = n_sp02;
            // 心率和血氧浓度初始数据打印
            //            printf("HR=%d, spo2:%d\r\n", n_heart_rate, n_sp02);
            break;
        }
    }
}

HRM_Mode current_mode;
int checkout_spo2_flag, checkout_prox_flag;
BloodData g_blooddata = {0}; // 血液数据存储

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

void blood_Loop(void)
{
    // 血液信息获取
    blood_data_update_proximity();

    if (current_mode == HRM_SPO2) {
        // max30102_ecg_init();
        max30102_data_handle(&n_heart_rate, &n_sp02);
        printf("heart:%d, sp02:%d\r\n", n_heart_rate, n_sp02);
        g_blooddata.heart = n_heart_rate;
        g_blooddata.SpO2  = n_sp02;
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
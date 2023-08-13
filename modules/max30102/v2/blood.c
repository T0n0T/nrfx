#include "blood.h"
#include "max30102.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>

#define DBG_LEVEL DBG_LOG
#define LOG_TAG   "blood.cal"
#include <rtdbg.h>

#define MAX_BRIGHTNESS 255

HRM_Mode current_mode;
int checkout_spo2_flag, checkout_prox_flag, translate_flag;
// rt_tick_t init_tick, cur_tick;

int32_t hr_val, spo2_val;

uint16_t g_fft_index = 0; // fft输入输出下标

uint32_t aun_ir_buffer[500];  // IR LED sensor data
int32_t n_ir_buffer_length;   // data length
uint32_t aun_red_buffer[500]; // Red LED sensor data
int32_t n_sp02;               // SPO2 value
int8_t ch_spo2_valid;         // indicator to show if the SP02 calculation is valid
int32_t n_heart_rate;         // heart rate value
int8_t ch_hr_valid;           // indicator to show if the heart rate calculation is valid
uint8_t uch_dummy;
int heart = 0;

void max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
    uint32_t un_temp;
    unsigned char uch_temp;
    char ach_i2c_data[6];
    *pun_red_led = 0;
    *pun_ir_led  = 0;

    // read and clear status register
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

    un_temp = (unsigned char)ach_i2c_data[0];
    un_temp <<= 16;
    *pun_red_led += un_temp;
    un_temp = (unsigned char)ach_i2c_data[1];
    un_temp <<= 8;
    *pun_red_led += un_temp;
    un_temp = (unsigned char)ach_i2c_data[2];
    *pun_red_led += un_temp;

    un_temp = (unsigned char)ach_i2c_data[3];
    un_temp <<= 16;
    *pun_ir_led += un_temp;
    un_temp = (unsigned char)ach_i2c_data[4];
    un_temp <<= 8;
    *pun_ir_led += un_temp;
    un_temp = (unsigned char)ach_i2c_data[5];
    *pun_ir_led += un_temp;
    *pun_red_led &= 0x03FFFF; // Mask MSB [23:18]
    *pun_ir_led &= 0x03FFFF;  // Mask MSB [23:18]
}

void max30102_Read_Data(int32_t *heart_rate, int32_t *sp02)
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
            while (rt_pin_read(cfg.irq_pin.pin) == 1)
                ;
            max30102_read_fifo(&aun_red_buffer[i], &aun_ir_buffer[i]);
            LOG_D("mode get data[%d]:  fifo_ir: %06d fifo_red: %06d", i, aun_ir_buffer[i], aun_red_buffer[i]);
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
        printf("HR=%d, spo2:%d\r\n", n_heart_rate, n_sp02);
        if ((ch_hr_valid == 1) && (ch_spo2_valid == 1) && (n_heart_rate > 50) && (n_heart_rate < 120) && (n_sp02 < 101) && (n_sp02 > 80)) //**/ ch_hr_valid == 1 && ch_spo2_valid ==1 && n_heart_rate<120 && n_sp02<101
        {
            *heart_rate = n_heart_rate;
            *sp02       = n_sp02;
            // 心率和血氧浓度初始数据打印
            // printf("HR=%d, spo2:%d\r\n", n_heart_rate, n_sp02);
            break;
        }
    }
}

void blood_data_update_proximity(void)
{
    // 模式切换
    int retry         = 8;
    uint8_t reg       = 0;
    uint8_t read_ptr  = 0;
    uint8_t write_ptr = 0;
    uint32_t fifo_red = 0;
    uint32_t fifo_ir  = 0;
    while (rt_pin_read(cfg.irq_pin.pin) == 0) {
        while (retry--) {
            max30102_read_fifo(&fifo_red, &fifo_ir); // read from MAX30102 FIFO2
            LOG_D("mode get data[%d]:  fifo_ir: %06d fifo_red: %06d", 8 - retry, fifo_ir, fifo_red);
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
        max30102_Read_Data(&hr_val, &spo2_val);
    }
}

void blood_Loop(void)
{
    // 血液信息获取
    blood_data_update_proximity();

    if (current_mode == HRM_SPO2) {
        printf("HR=%d, spo2:%d\r\n", hr_val, spo2_val);
    }
}

void max30102_thread_entry(void *args)
{
    checkout_spo2_flag = 0;
    checkout_prox_flag = 0;
    translate_flag     = 0;
    current_mode       = PROX;
    max30102_checkout_HRM_SPO2_mode();
    rt_pin_mode(cfg.irq_pin.pin, PIN_MODE_INPUT_PULLUP);
    while (1) {
        // blood_Loop();
        max30102_Read_Data(&hr_val, &spo2_val);
        printf("HR=%d, spo2:%d\r\n", hr_val, spo2_val);
        LOG_D("-------------------------------------------");
        rt_thread_mdelay(5);
    }
}
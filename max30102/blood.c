#include "blood.h"
#include "max30102.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>

#define DBG_LEVEL DBG_INFO
#define LOG_TAG   "blood.cal"
#include <rtdbg.h>

HRM_Mode current_mode;
int checkout_flag, translate_flag;
rt_tick_t init_tick, cur_tick;

uint16_t g_fft_index = 0; // fft输入输出下标
uint16_t fifo_red;
uint16_t fifo_ir;
uint8_t ach_i2c_data[6];
struct compx s1[FFT_N + 16]; // FFT输入和输出：从S[1]开始存放，根据大小自己定义
struct compx s2[FFT_N + 16]; // FFT输入和输出：从S[1]开始存放，根据大小自己定义

struct
{
    float Hp;   // 血红蛋白
    float HpO2; // 氧合血红蛋白

} g_BloodWave; // 血液波形数据

BloodData g_blooddata = {0}; // 血液数据存储

#define CORRECTED_VALUE 47 // 标定血液氧气含量

/*funcation start ------------------------------------------------------------*/
void max30102_read_fifo(void)
{
    uint16_t un_temp;
    uint8_t uch_temp;
    fifo_red = 0;
    fifo_ir  = 0;
    rt_memcpy(ach_i2c_data, 0, ARRAY_SIZE(ach_i2c_data));
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

    un_temp = ach_i2c_data[0];
    un_temp <<= 14;
    fifo_red += un_temp;
    un_temp = ach_i2c_data[1];
    un_temp <<= 6;
    fifo_red += un_temp;
    un_temp = ach_i2c_data[2];
    un_temp >>= 2;
    fifo_red += un_temp;

    un_temp = ach_i2c_data[3];
    un_temp <<= 14;
    fifo_ir += un_temp;
    un_temp = ach_i2c_data[4];
    un_temp <<= 6;
    fifo_ir += un_temp;
    un_temp = ach_i2c_data[5];
    un_temp >>= 2;
    fifo_ir += un_temp;
    // if (fifo_ir <= 10000) {
    //     fifo_ir = 0;
    // }
    // if (fifo_red <= 10000) {
    //     fifo_red = 0;
    // }
}

// 血液检测信息更新
void blood_data_update(void)
{
    // 标志位被使能时 读取FIFO
    g_fft_index = 0;
    while (g_fft_index < FFT_N) {
        {
            // 读取FIFO
            LOG_D("date get num[%d]:  fifo_ir: %06d fifo_red: %06d", g_fft_index, fifo_ir, fifo_red);
            max30102_read_fifo(); // read from MAX30102 FIFO2
            // 将数据写入fft输入并清除输出
            if (g_fft_index < FFT_N) {
                // 将数据写入fft输入并清除输出
                s1[g_fft_index].real = fifo_red;
                s1[g_fft_index].imag = 0;
                s2[g_fft_index].real = fifo_ir;
                s2[g_fft_index].imag = 0;
                g_fft_index++;
            }
        }
    }
}

void blood_data_update_proximity(void)
{
    while (rt_pin_read(cfg.irq_pin.pin) == 0) {
        max30102_read_fifo(); // read from MAX30102 FIFO2
        LOG_D("date get num[%d]:  fifo_ir: %06d fifo_red: %06d", g_fft_index, fifo_ir, fifo_red);
        if (fifo_ir > 10000 && current_mode == PROX) {
            if (checkout_flag > 10) {
                checkout_flag = 0;
                init_tick     = rt_tick_get();
                max30102_checkout_HRM_SPO2_mode();
                current_mode = HRM_SPO2;
                LOG_D("checkout into hrm_spo2.");
            }
            checkout_flag++;
            continue;
        } else if (fifo_ir < 5000 && current_mode == HRM_SPO2) {
            if (checkout_flag > 10) {
                checkout_flag = 0;
                max30102_checkout_proximity_mode();
                current_mode   = PROX;
                translate_flag = 0;
                LOG_D("checkout into prox_mode.");
            }
            checkout_flag++;
            break;
        }
        if (g_fft_index < FFT_N) {
            // 将数据写入fft输入并清除输出
            s1[g_fft_index].real = fifo_red;
            s1[g_fft_index].imag = 0;
            s2[g_fft_index].real = fifo_ir;
            s2[g_fft_index].imag = 0;
            g_fft_index++;
        } else {
            g_fft_index = 0;
        }
    }
}

// 血液信息转换
void blood_data_translate(void)
{
    float n_denom;
    uint16_t i;
    // 直流滤波
    float dc_red = 0;
    float dc_ir  = 0;
    float ac_red = 0;
    float ac_ir  = 0;

    for (i = 0; i < FFT_N; i++) {
        dc_red += s1[i].real;
        dc_ir += s2[i].real;
    }
    dc_red = dc_red / FFT_N;
    dc_ir  = dc_ir / FFT_N;
    for (i = 0; i < FFT_N; i++) {
        s1[i].real = s1[i].real - dc_red;
        s2[i].real = s2[i].real - dc_ir;
    }

    // 移动平均滤波
    // printf("\n***********8 pt Moving Average red******************************************************\n");
    //
    for (i = 1; i < FFT_N - 1; i++) {
        n_denom    = (s1[i - 1].real + 2 * s1[i].real + s1[i + 1].real);
        s1[i].real = n_denom / 4.00;

        n_denom    = (s2[i - 1].real + 2 * s2[i].real + s2[i + 1].real);
        s2[i].real = n_denom / 4.00;
    }
    // 八点平均滤波
    for (i = 0; i < FFT_N - 8; i++) {
        n_denom    = (s1[i].real + s1[i + 1].real + s1[i + 2].real + s1[i + 3].real + s1[i + 4].real + s1[i + 5].real + s1[i + 6].real + s1[i + 7].real);
        s1[i].real = n_denom / 8.00;

        n_denom    = (s2[i].real + s2[i + 1].real + s2[i + 2].real + s2[i + 3].real + s2[i + 4].real + s2[i + 5].real + s2[i + 6].real + s2[i + 7].real);
        s2[i].real = n_denom / 8.00;

        // printf("%f ", s1[i].real);
    }
    // printf("\n8 pt Moving Average ir***********************************************************************\n");
    // for (i = 0; i < FFT_N; i++) {
    //     printf("%f ", s2[i].real);
    // }
    // printf("\nAverage end *********************************************************************************\n");
    // 开始变换显示
    g_fft_index = 0;
    // 快速傅里叶变换
    FFT(s1);
    FFT(s2);
    // 解平方
    // printf("\nfft start************************************************************************************\n");
    for (i = 0; i < FFT_N; i++) {
        s1[i].real = sqrtf(s1[i].real * s1[i].real + s1[i].imag * s1[i].imag);
        s1[i].real = sqrtf(s2[i].real * s2[i].real + s2[i].imag * s2[i].imag);
    }
    // 计算交流分量
    for (i = 1; i < FFT_N; i++) {
        ac_red += s1[i].real;
        ac_ir += s2[i].real;
    }

    // for (i = 0; i < FFT_N / 2; i++) {
    //     printf("%f", s1[i].real);
    // }
    // printf("\n");
    // for (i = 0; i < FFT_N / 2; i++) {
    //     printf("%f", s2[i].real);
    // }

    // printf("\nfft end**************************************************************************************\n");
    int s1_max_index = find_max_num_index(s1, 30);
    int s2_max_index = find_max_num_index(s2, 30);
    printf("%d\n", s1_max_index);
    printf("%d\n", s2_max_index);
    float Heart_Rate = 60.00 * ((100.0 * s1_max_index) / 512.00) + 20;

    g_blooddata.heart = Heart_Rate;

    float R          = (ac_ir * dc_red) / (ac_red * dc_ir);
    float sp02_num   = -45.060 * R * R + 30.354 * R + 94.845;
    g_blooddata.SpO2 = sp02_num;
}

void blood_Loop(void)
{
    // 血液信息获取
    blood_data_update_proximity();

    if (current_mode == HRM_SPO2 && translate_flag) {
        // 血液信息转换
        blood_data_translate();
        // 显示血液状态信息
        g_blooddata.SpO2 = (g_blooddata.SpO2 > 99.99) ? 99.99 : g_blooddata.SpO2;
        printf("heart rate:\t%3d\n", g_blooddata.heart);
        printf("spO2:\t\t%0.2f\n", g_blooddata.SpO2);
    } else {
        cur_tick = rt_tick_get();
        if (cur_tick - init_tick - rt_tick_from_millisecond(5000) < RT_TICK_MAX / 2) {
            translate_flag = 1;
        }
    }
}

void max30102_thread_entry(void *args)
{
    checkout_flag  = 0;
    translate_flag = 0;
    current_mode   = PROX;

    rt_pin_mode(cfg.irq_pin.pin, PIN_MODE_INPUT_PULLUP);
    while (1) {
        blood_Loop();
        rt_thread_mdelay(15);
    }
}
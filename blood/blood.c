#include "blood.h"
#include "../inc/max30102.h"
#include <rtthread.h>
#include <rtdevice.h>

#define DBG_LEVEL DBG_LOG
#include <rtdbg.h>
#define LOG_TAG "blood.cal"

uint16_t g_fft_index = 0; // fft��������±�
uint16_t fifo_red;
uint16_t fifo_ir;
struct compx s1[FFT_N + 16]; // FFT������������S[1]��ʼ��ţ����ݴ�С�Լ�����
struct compx s2[FFT_N + 16]; // FFT������������S[1]��ʼ��ţ����ݴ�С�Լ�����

struct
{
    float Hp;   // Ѫ�쵰��
    float HpO2; // ����Ѫ�쵰��

} g_BloodWave; // ѪҺ��������

BloodData g_blooddata = {0}; // ѪҺ���ݴ洢

#define CORRECTED_VALUE 47 // �궨ѪҺ��������

/*funcation start ------------------------------------------------------------*/
void max30102_read_fifo(void)
{
    uint16_t un_temp;
    uint8_t uch_temp;
    fifo_red = 0;
    fifo_ir  = 0;
    uint8_t ach_i2c_data[6];
    maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_temp, sizeof(uch_temp));
    maxim_max30102_read_reg(REG_INTR_STATUS_2, &uch_temp, sizeof(uch_temp));

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

    if (fifo_ir <= 10000) {
        fifo_ir = 0;
    }
    if (fifo_red <= 10000) {
        fifo_red = 0;
    }
}

// ѪҺ�����Ϣ����
void blood_data_update(void)
{
    // ��־λ��ʹ��ʱ ��ȡFIFO
    g_fft_index = 0;
    while (g_fft_index < FFT_N) {
        while (rt_pin_read(cfg.irq_pin.pin) == 0) {
            // ��ȡFIFO
            max30102_read_fifo(); // read from MAX30102 FIFO2
            // ������д��fft���벢������
            if (g_fft_index < FFT_N) {
                // ������д��fft���벢������
                s1[g_fft_index].real = fifo_red;
                s1[g_fft_index].imag = 0;
                s2[g_fft_index].real = fifo_ir;
                s2[g_fft_index].imag = 0;
                g_fft_index++;
            }
        }
    }
}

// ѪҺ��Ϣת��
void blood_data_translate(void)
{
    float n_denom;
    uint16_t i;
    // ֱ���˲�
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

    // �ƶ�ƽ���˲�
    LOG_D("***********8 pt Moving Average red******************************************************");
    //
    for (i = 1; i < FFT_N - 1; i++) {
        n_denom    = (s1[i - 1].real + 2 * s1[i].real + s1[i + 1].real);
        s1[i].real = n_denom / 4.00;

        n_denom    = (s2[i - 1].real + 2 * s2[i].real + s2[i + 1].real);
        s2[i].real = n_denom / 4.00;
    }
    // �˵�ƽ���˲�
    for (i = 0; i < FFT_N - 8; i++) {
        n_denom    = (s1[i].real + s1[i + 1].real + s1[i + 2].real + s1[i + 3].real + s1[i + 4].real + s1[i + 5].real + s1[i + 6].real + s1[i + 7].real);
        s1[i].real = n_denom / 8.00;

        n_denom    = (s2[i].real + s2[i + 1].real + s2[i + 2].real + s2[i + 3].real + s2[i + 4].real + s2[i + 5].real + s2[i + 6].real + s2[i + 7].real);
        s2[i].real = n_denom / 8.00;

        LOG_D("%f", s1[i].real);
    }
    LOG_D("************8 pt Moving Average ir*************************************************************");
    for (i = 0; i < FFT_N; i++) {
        LOG_D("%f", s2[i].real);
    }
    LOG_D("**************************************************************************************************");
    // ��ʼ�任��ʾ
    g_fft_index = 0;
    // ���ٸ���Ҷ�任
    FFT(s1);
    FFT(s2);
    // ��ƽ��
    LOG_D("��ʼFFT�㷨****************************************************************************************************");
    for (i = 0; i < FFT_N; i++) {
        s1[i].real = sqrtf(s1[i].real * s1[i].real + s1[i].imag * s1[i].imag);
        s1[i].real = sqrtf(s2[i].real * s2[i].real + s2[i].imag * s2[i].imag);
    }
    // ���㽻������
    for (i = 1; i < FFT_N; i++) {
        ac_red += s1[i].real;
        ac_ir += s2[i].real;
    }

    for (i = 0; i < FFT_N / 2; i++) {
        LOG_D("%f", s1[i].real);
    }
    LOG_D("****************************************************************************************");
    for (i = 0; i < FFT_N / 2; i++) {
        LOG_D("%f", s2[i].real);
    }

    LOG_D("����FFT�㷨***************************************************************************************************************");
    //	for(i = 0;i < 50;i++)
    //	{
    //		if(s1[i].real<=10)
    //			break;
    //	}

    // UsartPrintf(USART_DEBUG,"%d",(int)i);
    // ��ȡ��ֵ��ĺ�����  �������������Ϊ
    int s1_max_index = find_max_num_index(s1, 30);
    int s2_max_index = find_max_num_index(s2, 30);
    LOG_D("%d", s1_max_index);
    LOG_D("%d", s2_max_index);
    float Heart_Rate = 60.00 * ((100.0 * s1_max_index) / 512.00) + 20;

    g_blooddata.heart = Heart_Rate;

    float R          = (ac_ir * dc_red) / (ac_red * dc_ir);
    float sp02_num   = -45.060 * R * R + 30.354 * R + 94.845;
    g_blooddata.SpO2 = sp02_num;
}

void blood_Loop(void)
{
    // ѪҺ��Ϣ��ȡ
    blood_data_update();
    // ѪҺ��Ϣת��
    blood_data_translate();
    // ��ʾѪҺ״̬��Ϣ
    g_blooddata.SpO2 = (g_blooddata.SpO2 > 99.99) ? 99.99 : g_blooddata.SpO2;
    rt_kprintf("heart rate:\t%3d\n", g_blooddata.heart);
    rt_kprintf("spO2:\t%0.2f\n", g_blooddata.SpO2);
}

void max30102_thread_entry(void *args)
{
    // for (int i = 0; i < 128; i++) {
    //     while (rt_pin_read(cfg.irq_pin.pin) == 0) {
    //         // ��ȡFIFO
    //         max30102_read_fifo();
    //     }
    // }
    while (1) {
        rt_kprintf("in------------------------");
        blood_Loop();
        rt_thread_mdelay(5);
    }
}
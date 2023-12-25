#include "blood.h"
#include "max30102.h"

#define NRF_LOG_MODULE_NAME BLOOD
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();
/**
  ******************************************************************************
  * 使用实例:
             int32_t n_heart_rate = 0;
             int32_t n_sp02 = 0;

             max30102_ecg_init();

             while(1)
             {
                 max30102_data_handle(&n_heart_rate, &n_sp02);            //
                 NRF_LOG_INFO("heart:%d, sp02:%d\r\n", n_heart_rate, n_sp02);
             }

  *
  ******************************************************************************
  */

#define MAX_BRIGHTNESS 255
#define SAMPLE_NUM     5
uint32_t aun_ir_buffer[500];  // IR LED sensor data
int32_t  n_ir_buffer_length;  // data length
uint32_t aun_red_buffer[500]; // Red LED sensor data

uint8_t uch_dummy;
uint8_t change_flag;
uint8_t ecg_status;
int32_t xheart;
int32_t xsp02;

static void max30102_read_fifo(uint32_t* pun_red_led, uint32_t* pun_ir_led);

void max30102_ecg_init(void)
{
    uint32_t un_min, un_max;
    uint32_t un_prev_data;
    int32_t  n_sp02;        // SPO2 value
    int8_t   ch_spo2_valid; // indicator to show if the SP02 calculation is valid
    int32_t  n_heart_rate;  // heart rate value
    int8_t   ch_hr_valid;   // indicator to show if the heart rate calculation is valid
    int      i;

    maxim_max30102_write_reg(REG_INTR_ENABLE_1, 0xc0); // INTR setting
    maxim_max30102_write_reg(REG_INTR_ENABLE_1, 0xc0); // INTR setting
    maxim_max30102_write_reg(REG_INTR_ENABLE_2, 0x00);
    maxim_max30102_write_reg(REG_FIFO_WR_PTR, 0x00); // FIFO_WR_PTR[4:0]
    maxim_max30102_write_reg(REG_OVF_COUNTER, 0x00); // OVF_COUNTER[4:0]
    maxim_max30102_write_reg(REG_FIFO_RD_PTR, 0x00); // FIFO_RD_PTR[4:0]
    maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x0f); // sample avg = 1, fifo rollover=false, fifo almost full = 17
    maxim_max30102_write_reg(REG_MODE_CONFIG, 0x03); // 0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
    maxim_max30102_write_reg(REG_SPO2_CONFIG, 0x27); // SPO2_ADC range = 4096nA, SPO2 sample rate (100 Hz), LED pulseWidth (400uS)
    maxim_max30102_write_reg(REG_LED1_PA, 0xff);     // Choose value for ~ 7mA for LED1
    maxim_max30102_write_reg(REG_LED2_PA, 0x7f);     // Choose value for ~ 7mA for LED2
    maxim_max30102_write_reg(REG_PILOT_PA, 0x7f);    // Choose value for ~ 25mA for Pilot LED
    un_min             = 0x3FFFF;
    un_max             = 0;
    n_ir_buffer_length = 500; // buffer length of 100 stores 5 seconds of samples running at 100sps

    for (i = 0; i < n_ir_buffer_length; i++) {
        while (nrf_gpio_pin_read(MAX_PIN_INT) == 1)
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

static void max30102_read_fifo(uint32_t* pun_red_led, uint32_t* pun_ir_led)
{
    uint32_t un_temp;
    uint8_t  uch_temp;
    uint8_t  ach_i2c_data[6] = {0};

    if (!maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_temp, sizeof(uch_temp))) {
        NRF_LOG_ERROR("Max30102 clean intr1 failed.");
        return;
    }
    if (!maxim_max30102_read_reg(REG_INTR_STATUS_2, &uch_temp, sizeof(uch_temp))) {
        NRF_LOG_ERROR("Max30102 clean intr2 failed.");
        return;
    }

    if (!maxim_max30102_read_reg(REG_FIFO_DATA, ach_i2c_data, ARRAY_SIZE(ach_i2c_data))) {
        NRF_LOG_ERROR("Max30102 read fifo failed.");
        return;
    }

    if (pun_red_led != 0) {
        *pun_red_led = (long)((long)((long)ach_i2c_data[0] & 0x03) << 16) | (long)ach_i2c_data[1] << 8 | (long)ach_i2c_data[2]; // Combine values to get the actual number
    }

    if (pun_ir_led != 0) {
        *pun_ir_led = (long)((long)((long)ach_i2c_data[3] & 0x03) << 16) | (long)ach_i2c_data[4] << 8 | (long)ach_i2c_data[5]; // Combine values to get the actual number
    }
}

int max30102_data_handle(int32_t* heart_rate, int32_t* sp02)
{
    // variables to calculate the on-board LED brightness that reflects the heartbeats
    uint32_t un_min, un_max, un_prev_data;
    int      i;
    int      retry = 10;
    float    f_temp;
    int32_t  n_brightness;

    int     s_ecg_status;
    int32_t n_sp02;        // SPO2 value
    int8_t  ch_spo2_valid; // indicator to show if the SP02 calculation is valid
    int32_t n_heart_rate;  // heart rate value
    int8_t  ch_hr_valid;   // indicator to show if the heart rate calculation is valid

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
            while (nrf_gpio_pin_read(MAX_PIN_INT)) {
            }

            max30102_read_fifo(&aun_red_buffer[i], &aun_ir_buffer[i]);
            // NRF_LOG_DEBUG("[%d]: aun_red_buffer = %d, aun_ir_buffer = %d", i, aun_red_buffer[i], aun_ir_buffer[i]);
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
        }

        ch_hr_valid   = 0;
        ch_spo2_valid = 0;
        n_heart_rate  = 0;
        n_sp02        = 0;
        s_ecg_status  = 0;
        maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, aun_red_buffer, &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
        n_heart_rate = n_heart_rate / 5;
        if ((ch_hr_valid == 1) && (ch_spo2_valid == 1) && (n_heart_rate > 60) && (n_heart_rate < 120) && (n_sp02 < 101) && (n_sp02 > 80)) //**/ ch_hr_valid == 1 && ch_spo2_valid ==1 && n_heart_rate<120 && n_sp02<101
        {
            *heart_rate = n_heart_rate;
            *sp02       = n_sp02;
            change_flag++;
            if (change_flag > 3) {
                change_flag  = 3;
                s_ecg_status = 1;
                NRF_LOG_DEBUG("ecg_status change: %d", s_ecg_status);
            }

            break;
        } else if (!retry) {
            change_flag--;
            if (change_flag < 0) {
                change_flag  = 0;
                s_ecg_status = 0;
                NRF_LOG_DEBUG("ecg_status change: %d", s_ecg_status);
            }

            break;
        } else {
            retry--;
        }
    }
    NRF_LOG_DEBUG("HR=%d, spo2:%d, s_ecg_status: %d", n_heart_rate, n_sp02, s_ecg_status);

    return s_ecg_status;
}

HRM_Mode current_mode;
int      checkout_spo2_flag, checkout_prox_flag;

void blood_data_update_proximity(void)
{
    int      retry     = 8;
    uint8_t  reg       = 0;
    uint8_t  read_ptr  = 0;
    uint8_t  write_ptr = 0;
    uint32_t fifo_ir;
    while (nrf_gpio_pin_read(MAX_PIN_INT) == 0) {
        while (retry--) {
            max30102_read_fifo(0, &fifo_ir); // read from MAX30102 FIFO2
            NRF_LOG_DEBUG("mode get data[%d]:  fifo_ir: %06d", 8 - retry, fifo_ir);
            if (fifo_ir > 5000 && current_mode == PROX) {
                if (checkout_spo2_flag > 3) {
                    checkout_spo2_flag = 0;
                    // max30102_checkout_HRM_SPO2_mode();
                    current_mode = HRM_SPO2;
                    NRF_LOG_DEBUG("checkout into hrm_spo2.");
                    goto MODE_ENTRY;
                }
                checkout_prox_flag = 0;
                checkout_spo2_flag++;
                continue;
            } else if (fifo_ir < 10000 && current_mode == HRM_SPO2) {
                if (checkout_prox_flag > 3) {
                    checkout_prox_flag = 0;
                    maxim_max30102_init_proximity_mode();
                    current_mode = PROX;
                    ecg_status   = 0;
                    NRF_LOG_DEBUG("checkout into prox_mode.");
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
    if (current_mode == HRM_SPO2) {
        NRF_LOG_DEBUG("HRM_SPO2_MODE!!!!!!!!!!!!!!!!!!!");
    }
}

void blood_Loop(void)
{
    ecg_status            = 0;
    int      temp         = 0;
    uint32_t temp_hr      = 0;
    uint32_t temp_sp02    = 0;
    int32_t  n_sp02       = 0;
    int32_t  n_heart_rate = 0;

    max30102_ecg_init();
    for (size_t i = 0; i < SAMPLE_NUM; i++) {
        int32_t n_sp02       = 0;
        int32_t n_heart_rate = 0;
        temp += max30102_data_handle(&n_heart_rate, &n_sp02);
        temp_hr += n_heart_rate;
        temp_sp02 += n_sp02;
    }
    ecg_status = temp > 4 ? 1 : 0;
    xheart     = temp_hr / SAMPLE_NUM;
    xsp02      = temp_sp02 / SAMPLE_NUM;
    // NRF_LOG_INFO("--------------------heart_rate: %d, sp02: %d, ecg: %d", xheart, xsp02, ecg_status);
    maxim_max30102_reset();
}

void max30102_thread_entry(void* args)
{
    ecg_status = 0;

    int32_t n_heart_rate = 0;
    int32_t n_sp02       = 0;

    while (1) {
        blood_Loop();
        vTaskDelay(10000);
    }
}
/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-02-08     Jackistang   the first version
 */
#include "max30102.h"
#include <rtthread.h>
#include <rtdevice.h>

#define DBG_LEVEL DBG_LOG
#include <rtdbg.h>
#define LOG_TAG "sensor.max30102"

static struct rt_i2c_bus_device *i2c_bus;
static rt_uint32_t g_heartrate;

void max30102_thread_entry(void *args);

/**
 * \brief        Write a value to a MAX30102 register
 * \par          Details
 *               This function writes a value to a MAX30102 register
 *
 * \param[in]    uch_addr    - register address
 * \param[in]    uch_data    - register data
 *
 * \retval       true on success
 */
rt_bool_t maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data)
{
    uint8_t buf[2];
    struct rt_i2c_msg msg;
    rt_size_t err;

    buf[0] = uch_addr;
    buf[1] = uch_data;

    msg.addr  = MAX30102_ADDR;
    msg.flags = RT_I2C_WR;
    msg.buf   = buf;
    msg.len   = ARRAY_SIZE(buf);

    if ((err = rt_i2c_transfer(i2c_bus, &msg, 1)) != 1) {
        LOG_E("I2c write failed (err: %d).", err);
        return RT_FALSE;
    }

    return RT_TRUE;
}

/**
 * \brief        Read a MAX30102 register
 * \par          Details
 *               This function reads a MAX30102 register
 *
 * \param[in]    uch_addr    - register address
 * \param[out]   puch_data   - pointer that stores the register data
 *
 * \retval       true on success
 */
rt_bool_t maxim_max30102_read_reg(uint8_t uch_addr, uint8_t *data, uint16_t len)
{
    RT_ASSERT(data);

    struct rt_i2c_msg msg_buf[2];
    rt_size_t err;

    msg_buf[0].addr  = MAX30102_ADDR;
    msg_buf[0].flags = RT_I2C_WR;
    msg_buf[0].buf   = &uch_addr;
    msg_buf[0].len   = 1;

    msg_buf[1].addr  = MAX30102_ADDR;
    msg_buf[1].flags = RT_I2C_RD;
    msg_buf[1].buf   = data;
    msg_buf[1].len   = len;

    if ((err = rt_i2c_transfer(i2c_bus, msg_buf, 2)) != 2) {
        LOG_E("I2c read failed (err: %d).", err);
        rt_i2c_control(i2c_bus, 1, 0);
        return RT_FALSE;
    }

    return RT_TRUE;
}

/**
 * \brief        Initialize the MAX30102
 * \par          Details
 *               This function initializes the MAX30102
 *
 * \param        None
 *
 * \retval       true on success
 */
static rt_bool_t maxim_max30102_init()
{
    if (!maxim_max30102_write_reg(REG_INTR_ENABLE_1, 0x40)) // INTR setting: PPG new/fifo full
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_INTR_ENABLE_2, 0x00))
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_WR_PTR, 0x00)) // FIFO_WR_PTR[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_OVF_COUNTER, 0x00)) // OVF_COUNTER[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_RD_PTR, 0x00)) // FIFO_RD_PTR[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x4f)) // sample avg = 4, fifo rollover=RT_FALSE, fifo almost full = 17
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x03)) // 0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_SPO2_CONFIG, 0x27)) // SPO2_ADC range = 4096nA, SPO2 sample rate (100 Hz), LED pulseWidth (411uS)
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_LED1_PA, 0x24)) // Choose value for ~ 7mA for LED1
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_LED2_PA, 0x24)) // Choose value for ~ 7mA for LED2
        return RT_FALSE;

    return RT_TRUE;
}

static rt_bool_t maxim_max30102_init_proximity_mode()
{
    if (!maxim_max30102_write_reg(REG_FIFO_WR_PTR, 0x00)) // FIFO_WR_PTR[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_OVF_COUNTER, 0x00)) // OVF_COUNTER[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_RD_PTR, 0x00)) // FIFO_RD_PTR[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x1E)) // sample avg = 1, fifo rollover=1, 16 unread samlpes,8 gourps
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x07)) // 0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_LED1_PA, 0x19)) // LED1: 5mA
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_LED2_PA, 0x00)) // LED2: 0mA
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_SPO2_CONFIG, 0x43))     // SPO2_ADC_RGE : 8uA (this is a good starting point, try other settings in order to fine tune your design),
        return RT_FALSE;                                      // SPO2_SR : 50HZ, LED_PW : 400us
    if (!maxim_max30102_write_reg(REG_MULTI_LED_CTRL1, 0x12)) // Using LED1 in IR,LED2 in RED
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_MULTI_LED_CTRL2, 0x00))
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_INTR_ENABLE_1, 0xc0)) // INTR setting: PPG int
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_INTR_ENABLE_2, 0x00))
        return RT_FALSE;
    return RT_TRUE;
}

rt_bool_t max30102_checkout_HRM_SPO2_mode()
{
    if (!maxim_max30102_write_reg(REG_LED1_PA, 0x24)) // LED1: 7mA
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_LED2_PA, 0x24)) // LED2: 7mA
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x0f)) // sample avg = 1, fifo rollover=false, fifo almost full = 17
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_SPO2_CONFIG, 0x27)) // adc rang 4096; sample rate 100hz; led_width 400us = 2.5khz
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x03))
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_PILOT_PA, 0x7f))
        return RT_FALSE;
    return RT_TRUE;
}

rt_bool_t max30102_checkout_proximity_mode()
{
    if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x07)) // 0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_LED1_PA, 0x19)) // LED1: 5mA
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_LED2_PA, 0x00)) // LED2: 0mA
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_SPO2_CONFIG, 0x43)) // SPO2_ADC_RGE : 8uA (this is a good starting point, try other settings in order to fine tune your design),
        return RT_FALSE;                                  // SPO2_SR : 50HZ, LED_PW : 400us
    if (!maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x1E)) // sample avg = 1, fifo rollover=1, 16 unread samlpes,8 gourps
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_WR_PTR, 0x00)) // FIFO_WR_PTR[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_OVF_COUNTER, 0x00)) // OVF_COUNTER[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_RD_PTR, 0x00)) // FIFO_RD_PTR[4:0]
        return RT_FALSE;
    if (!maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x1E)) // sample avg = 1, fifo rollover=1, 16 unread samlpes,8 gourps
        return RT_FALSE;
    return RT_TRUE;
}
/**
 * \brief        Read a set of samples from the MAX30102 FIFO register
 * \par          Details
 *               This function reads a set of samples from the MAX30102 FIFO register
 *
 * \param[out]   *pun_red_led   - pointer that stores the red LED reading data
 * \param[out]   *pun_ir_led    - pointer that stores the IR LED reading data
 *
 * \retval       true on success
 */
rt_bool_t maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
    uint8_t uch_temp;
    uint8_t buf[6];

    *pun_ir_led  = 0;
    *pun_red_led = 0;

    maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_temp, sizeof(uch_temp));
    maxim_max30102_read_reg(REG_INTR_STATUS_2, &uch_temp, sizeof(uch_temp));

    if (!maxim_max30102_read_reg(REG_FIFO_DATA, buf, ARRAY_SIZE(buf))) {
        LOG_E("Max30102 read fifo failed.");
        return RT_FALSE;
    }

    *pun_red_led = (((uint32_t)buf[0] << 16) + ((uint32_t)buf[1] << 8) + buf[2]);
    *pun_ir_led  = (((uint32_t)buf[3] << 16) + ((uint32_t)buf[4] << 8) + buf[5]);

    *pun_red_led &= 0x03FFFF; // Mask MSB [23:18]
    *pun_ir_led &= 0x03FFFF;  // Mask MSB [23:18]

    return RT_TRUE;
}

/**
 * \brief        Reset the MAX30102
 * \par          Details
 *               This function resets the MAX30102
 *
 * \param        None
 *
 * \retval       true on success
 */
rt_bool_t maxim_max30102_reset()
{
    if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x40)) {
        LOG_E("Max30102 reset failed.");
        return RT_FALSE;
    }

    return RT_TRUE;
}

static rt_size_t max30102_fetch_data(struct rt_sensor_device *sensor, void *buf, rt_size_t len)
{
    RT_ASSERT(sensor);
    RT_ASSERT(buf);
    RT_ASSERT(len == sizeof(struct rt_sensor_data));

    struct rt_sensor_data *data = buf;
    data->timestamp             = rt_tick_get();
    data->type                  = RT_SENSOR_UNIT_BPM;
    data->data.hr               = g_heartrate;

    return len;
}

static rt_err_t max30102_control(struct rt_sensor_device *sensor, int cmd, void *arg)
{
    RT_ASSERT(sensor);
    RT_ASSERT(arg);

    if (cmd == RT_SENSOR_CTRL_SET_MODE || cmd == RT_SENSOR_CTRL_SET_POWER)
        return RT_EOK;

    LOG_E("Now support command: (%d).", cmd);
    return RT_ERROR;
}

static struct rt_sensor_ops max30102_ops = {
    .fetch_data = max30102_fetch_data,
    .control    = max30102_control,
};

static struct rt_thread max_thread             = {0};
static char max_stack[768]                     = {0};
static struct rt_sensor_device max30102_sensor = {0};

int rt_hw_max30102_init(struct rt_sensor_config *cfg)
{
    RT_ASSERT(cfg);
    RT_ASSERT(cfg->mode == RT_SENSOR_MODE_POLLING); // Only support polling.

    char *err_msg      = RT_NULL;
    rt_sensor_t sensor = RT_NULL;

    do {
        i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(cfg->intf.dev_name);
        if (i2c_bus == RT_NULL) {
            err_msg = "Not find i2c bus (cfg->intf.dev_name).";
            goto exit;
        }
        struct rt_sensor_info info = {
            .type       = RT_SENSOR_CLASS_HR,
            .vendor     = RT_SENSOR_VENDOR_MAXIM,
            .model      = "max30102",
            .unit       = RT_SENSOR_UNIT_BPM,
            .intf_type  = RT_SENSOR_INTF_I2C,
            .range_max  = 200, // 200 bpm
            .range_min  = 30,  // 30 bpm
            .period_min = 1,   // 1 ms
        };
        rt_memcpy(&max30102_sensor.info, &info, sizeof(struct rt_sensor_info));
        rt_memcpy(&max30102_sensor.config, cfg, sizeof(struct rt_sensor_config));
        max30102_sensor.ops = &max30102_ops;

        if (rt_hw_sensor_register(&max30102_sensor, "max30102", RT_DEVICE_FLAG_RDONLY, RT_NULL) != RT_EOK) {
            err_msg = "Register max30102 sensor device error.";
            goto exit;
        }

        // MAX30102 Init.
        rt_kprintf("[max30102] init max30102 sensor...\n");
        uint8_t uch_dummy;
        if (!maxim_max30102_reset()) {
            err_msg = "reset max30102 error.";
            goto exit;
        } // resets the MAX30102
        rt_thread_mdelay(1000);

        if (!maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_dummy, sizeof(uch_dummy))) {
            err_msg = "clean max30102 interrupt status error.";
            goto exit;
        } // Reads/clears the interrupt status register
        // maxim_max30102_init();                                                     // initialize the MAX30102
        if (!maxim_max30102_init_proximity_mode()) {
            err_msg = "set max30102 proximity_mode error.";
            goto exit;
        } // initialize the MAX30102

        if (rt_thread_init(&max_thread, "max30102", max30102_thread_entry, RT_NULL, max_stack, sizeof(max_stack), 21, 10) != RT_EOK) {
            err_msg = "max30102 thread init fail";
            goto exit;
        } else {
            rt_thread_startup(&max_thread);
        }

    } while (0);

exit:
    if (err_msg) {
        LOG_E(err_msg);
        return RT_ERROR;
    }
    return RT_EOK;
}

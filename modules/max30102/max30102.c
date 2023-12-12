/**
 * @file max30102.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "max30102.h"
#include <nrfx_twim.h>
#include <nrfx_twi_twim.h>

#define NRF_LOG_MODULE_NAME max30102
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static TaskHandle_t       max30102_handle;
static volatile bool      m_xfer_done = false;
static const nrfx_twim_t  m_twim      = NRFX_TWIM_INSTANCE(0);
static nrfx_twim_config_t i2c_cfg     = NRFX_TWIM_DEFAULT_CONFIG;

bool maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data)
{
    ret_code_t err_code;
    uint8_t    tx_buf[2];
    uint16_t   retry = 0;
    tx_buf[0]        = uch_addr;
    tx_buf[1]        = uch_data;

    m_xfer_done = false;
    err_code    = nrfx_twim_tx(&m_twim, MAX30102_ADDR, tx_buf, 2, false);
    while (m_xfer_done == false) {
        retry++;
        if (retry > INT16_MAX) {
            NRF_LOG_ERROR("twim tx in write max30102 timeout");
            return false;
        }
    }
    if (NRF_SUCCESS != err_code) {
        return false;
    }
    return true;
}

bool maxim_max30102_read_reg(uint8_t uch_addr, uint8_t* data, uint16_t len)
{
    ret_code_t err_code;
    uint16_t   retry = 0;
    m_xfer_done      = false;
    err_code         = nrfx_twim_tx(&m_twim, MAX30102_ADDR, &uch_addr, 1, true);

    while (m_xfer_done == false) {
        retry++;
        if (retry > INT16_MAX) {
            NRF_LOG_ERROR("twim tx in read max30102 timeout");
            return false;
        }
    }
    if (NRF_SUCCESS != err_code) {
        return false;
    }

    m_xfer_done = false;
    err_code    = nrfx_twim_rx(&m_twim, MAX30102_ADDR, data, len);

    retry = 0;
    while (m_xfer_done == false) {
        retry++;
        if (retry > INT16_MAX) {
            NRF_LOG_ERROR("twim rx in read max30102 timeout");
            return false;
        }
    }
    if (NRF_SUCCESS != err_code) {
        return false;
    }
    return true;
}

bool maxim_max30102_reset()
{
    if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x40)) {
        return false;
    }
    return true;
}

bool maxim_max30102_init_proximity_mode()
{
    if (!maxim_max30102_write_reg(REG_FIFO_WR_PTR, 0x00)) // FIFO_WR_PTR[4:0]
        return false;
    if (!maxim_max30102_write_reg(REG_OVF_COUNTER, 0x00)) // OVF_COUNTER[4:0]
        return false;
    if (!maxim_max30102_write_reg(REG_FIFO_RD_PTR, 0x00)) // FIFO_RD_PTR[4:0]
        return false;
    if (!maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x1E)) // sample avg = 1, fifo rollover=1, 16 unread samlpes,8 gourps
        return false;
    if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x07)) // 0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
        return false;
    if (!maxim_max30102_write_reg(REG_LED1_PA, 0x19)) // LED1: 5mA
        return false;
    if (!maxim_max30102_write_reg(REG_LED2_PA, 0x00)) // LED2: 0mA
        return false;
    if (!maxim_max30102_write_reg(REG_SPO2_CONFIG, 0x43))     // SPO2_ADC_RGE : 8uA (this is a good starting point, try other settings in order to fine tune your design),
        return false;                                         // SPO2_SR : 50HZ, LED_PW : 400us
    if (!maxim_max30102_write_reg(REG_MULTI_LED_CTRL1, 0x12)) // Using LED1 in IR,LED2 in RED
        return false;
    if (!maxim_max30102_write_reg(REG_MULTI_LED_CTRL2, 0x00))
        return false;
    if (!maxim_max30102_write_reg(REG_INTR_ENABLE_1, 0xc0)) // INTR setting: PPG int
        return false;
    if (!maxim_max30102_write_reg(REG_INTR_ENABLE_2, 0x00))
        return false;
    return true;
}

static void i2c_handle(nrfx_twim_evt_t const* p_event, void* p_context)
{
    switch (p_event->type) {
        case NRFX_TWIM_EVT_DONE: ///< Transfer completed event.
            m_xfer_done = true;
            NRF_LOG_DEBUG("i2c transfer done");
            break;
        case NRFX_TWIM_EVT_ADDRESS_NACK: ///< Error event: NACK received after sending the address.
            NRF_LOG_ERROR("i2c meet wrong of NACK received after sending the address");
            break;
        case NRFX_TWIM_EVT_DATA_NACK: ///< Error event: NACK received after sending a data byte.
            NRF_LOG_ERROR("i2c meet wrong of NACK received after sending a data byte");
            break;
        case NRFX_TWIM_EVT_OVERRUN: ///< Error event: The unread data is replaced by new data.
            NRF_LOG_ERROR("i2c meet wrong of The unread data is replaced by new data");
            break;
        case NRFX_TWIM_EVT_BUS_ERROR: ///< Error event: An unexpected transition occurred on the bus.
            NRF_LOG_ERROR("i2c meet wrong of An unexpected transition occurred on the bus");
            break;
        default:
            break;
    }
}

void twim_init(void)
{
    nrfx_twim_init(&m_twim, &i2c_cfg, i2c_handle, NULL);
    nrfx_twim_enable(&m_twim);
}

void twim_uninit(void)
{
    nrfx_twim_uninit(&m_twim);
}

void max30102_init(void)
{
    i2c_cfg.frequency = NRF_TWIM_FREQ_400K;
    i2c_cfg.scl       = MAX_PIN_SCL;
    i2c_cfg.sda       = MAX_PIN_SDA;
    twim_init();

    uint8_t uch_dummy;
    if (!maxim_max30102_reset()) {
        NRF_LOG_ERROR("reset max30102 error.");
        return;
    } // resets the MAX30102

    if (!maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_dummy, sizeof(uch_dummy))) {
        NRF_LOG_ERROR("clean max30102 interrupt status error.");
        return;
    } // Reads/clears the interrupt status register

    BaseType_t xReturned = xTaskCreate(max30102_thread_entry,
                                       "MAX30102",
                                       1024,
                                       0,
                                       configMAX_PRIORITIES - 3,
                                       &max30102_handle);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("MAX30102 task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
}
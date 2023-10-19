/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-15     xckhmf       First Verison
 * 2021-11-27     chenyingchun fix _master_xfer bug
 * 2023-01-28     Andrew       add Nrf5340 support
 */

#include <rtdevice.h>

#include <drv_i2c.h>
#include <hal/nrf_gpio.h>

#define LOG_TAG "drv.i2c"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#if defined(BSP_USING_I2C0) || defined(BSP_USING_I2C1) || defined(BSP_USING_I2C2) || defined(BSP_USING_I2C3)

#define TWI_TWIN_RETRY_TIME (1000000)
#define TWI_TWIM_WAIT(expression, retry, result)       \
    result = 0;                                        \
    while (expression) {                               \
        retry--;                                       \
        if (retry == 0) {                              \
            result = 1;                                \
            LOG_E("twim transfer timeout,  at: %s:%d", \
                  __FUNCTION__, __LINE__);             \
            break;                                     \
        }                                              \
    }

#define TWI_TWIM_PIN_CONFIGURE(_pin) nrf_gpio_cfg((_pin),                     \
                                                  NRF_GPIO_PIN_DIR_OUTPUT,    \
                                                  NRF_GPIO_PIN_INPUT_CONNECT, \
                                                  NRF_GPIO_PIN_PULLUP,        \
                                                  NRF_GPIO_PIN_S0D1,          \
                                                  NRF_GPIO_PIN_NOSENSE)

#ifdef BSP_USING_I2C0
static drv_i2c_cfg_t drv_i2c_0 =
    {
        .freq            = NRF_TWIM_FREQ_400K,
        .scl_pin         = BSP_I2C0_SCL_PIN,
        .sda_pin         = BSP_I2C0_SDA_PIN,
        .twi_instance    = NRFX_TWIM_INSTANCE(0),
        .transfer_status = false};
static struct rt_i2c_bus_device i2c0_bus;
nrfx_twim_t i2c0            = NRFX_TWIM_INSTANCE(0);
nrfx_twim_config_t i2c0_cfg = NRFX_TWIM_DEFAULT_CONFIG;

#endif
#ifdef BSP_USING_I2C1
static drv_i2c_cfg_t drv_i2c_1 =
    {
        .freq            = NRF_TWIM_FREQ_400K,
        .scl_pin         = BSP_I2C1_SCL_PIN,
        .sda_pin         = BSP_I2C1_SDA_PIN,
        .twi_instance    = NRFX_TWIM_INSTANCE(1),
        .transfer_status = false};
static struct rt_i2c_bus_device i2c1_bus;
#endif
#ifdef BSP_USING_I2C2
static drv_i2c_cfg_t drv_i2c_2 =
    {
        .freq            = NRF_TWIM_FREQ_400K,
        .scl_pin         = BSP_I2C2_SCL_PIN,
        .sda_pin         = BSP_I2C2_SDA_PIN,
        .twi_instance    = NRFX_TWIM_INSTANCE(2),
        .transfer_status = false};
static struct rt_i2c_bus_device i2c2_bus;
#endif
#ifdef BSP_USING_I2C3
static drv_i2c_cfg_t drv_i2c_3 =
    {
        .freq            = NRF_TWIM_FREQ_400K,
        .scl_pin         = BSP_I2C3_SCL_PIN,
        .sda_pin         = BSP_I2C3_SDA_PIN,
        .twi_instance    = NRFX_TWIM_INSTANCE(3),
        .transfer_status = false};
static struct rt_i2c_bus_device i2c3_bus;
#endif

rt_err_t _i2c_bus_control(struct rt_i2c_bus_device *bus, int cmd, void *args);

void i2c_handle(nrfx_twim_evt_t const *p_event, void *p_context)
{
    drv_i2c_cfg_t *p_cfg = (drv_i2c_cfg_t *)p_context;
    switch (p_event->type) {
        case NRFX_TWIM_EVT_DONE: ///< Transfer completed event.
            p_cfg->transfer_status = true;
            LOG_D("i2c transfer done");
            break;
        case NRFX_TWIM_EVT_ADDRESS_NACK: ///< Error event: NACK received after sending the address.
            LOG_E("i2c meet wrong of NACK received after sending the address");
            break;
        case NRFX_TWIM_EVT_DATA_NACK: ///< Error event: NACK received after sending a data byte.
            LOG_E("i2c meet wrong of NACK received after sending a data byte");
            break;
        case NRFX_TWIM_EVT_OVERRUN: ///< Error event: The unread data is replaced by new data.
            LOG_E("i2c meet wrong of The unread data is replaced by new data");
            break;
        case NRFX_TWIM_EVT_BUS_ERROR: ///< Error event: An unexpected transition occurred on the bus.
            LOG_E("i2c meet wrong of An unexpected transition occurred on the bus");
            break;
        default:
            break;
    }
}

static int twi_master_init(struct rt_i2c_bus_device *bus)
{
    nrfx_err_t rtn;

    drv_i2c_cfg_t *p_cfg          = bus->priv;
    nrfx_twim_t const *p_instance = &p_cfg->twi_instance;

    i2c0_cfg.frequency = p_cfg->freq;
    i2c0_cfg.scl       = p_cfg->scl_pin;
    i2c0_cfg.sda       = p_cfg->sda_pin;

    nrf_gpio_pin_set(i2c0_cfg.scl);
    nrf_gpio_pin_set(i2c0_cfg.sda);

    TWI_TWIM_PIN_CONFIGURE(i2c0_cfg.scl);
    TWI_TWIM_PIN_CONFIGURE(i2c0_cfg.sda);
    NRFX_DELAY_US(4);

    for (uint8_t i = 0; i < 9; i++) {
        if (nrf_gpio_pin_read(i2c0_cfg.sda)) {
            break;
        } else {
            // Pulse CLOCK signal
            nrf_gpio_pin_clear(i2c0_cfg.scl);
            NRFX_DELAY_US(4);
            nrf_gpio_pin_set(i2c0_cfg.scl);
            NRFX_DELAY_US(4);
        }
    }
    if (!nrf_gpio_pin_read(i2c0_cfg.sda)) {
        return NRFX_ERROR_INTERNAL;
    }

    rtn = nrfx_twim_init(p_instance, &i2c0_cfg, i2c_handle, p_cfg);
    // rtn = nrfx_twim_init(p_instance, &i2c0_cfg, NULL, p_cfg);
    if (rtn != NRFX_SUCCESS) {
        return rtn;
    }
    nrfx_twim_enable(p_instance);
    return 0;
}

static rt_ssize_t _master_xfer(struct rt_i2c_bus_device *bus,
                               struct rt_i2c_msg msgs[],
                               rt_uint32_t num)
{
    struct rt_i2c_msg *msg;
    nrfx_twim_t const *p_instance = &((drv_i2c_cfg_t *)bus->priv)->twi_instance;
    uint8_t *status               = &((drv_i2c_cfg_t *)bus->priv)->transfer_status;
    uint8_t result                = 0;
    uint8_t retry                 = 0;
    nrfx_err_t ret                = NRFX_SUCCESS;
    uint32_t no_stop_flag         = 0;
    rt_int32_t i                  = 0;

    for (i = 0; i < num; i++) {
        msg                        = &msgs[i];
        nrfx_twim_xfer_desc_t xfer = NRFX_TWIM_XFER_DESC_TX(msg->addr, msg->buf, msg->len);
        *status                    = false;
        if (msg->flags & RT_I2C_RD) {
            xfer.type = NRFX_TWIM_XFER_RX;
        } else {
            xfer.type    = NRFX_TWIM_XFER_TX;
            no_stop_flag = 0;
            if (msg->flags & RT_I2C_NO_READ_ACK) {
                no_stop_flag = NRFX_TWIM_FLAG_TX_NO_STOP;
            }
        }

        retry = TWI_TWIN_RETRY_TIME;
        TWI_TWIM_WAIT(nrfx_twim_is_busy(p_instance), retry, result);
        if (result) {
            nrf_twim_errorsrc_get_and_clear(p_instance->p_twim);
            goto out;
        }

        ret = nrfx_twim_xfer(p_instance, &xfer, no_stop_flag);

        retry = TWI_TWIN_RETRY_TIME;
        TWI_TWIM_WAIT(*status == false, retry, result);
        if (result) {
            nrf_twim_errorsrc_get_and_clear(p_instance->p_twim);
            goto out;
        }

        if (ret != NRFX_SUCCESS) {
            // printf("i2c transfer fail:err[%d]", ret);
            goto out;
        }
    }

out:
    return i;
}

rt_err_t _i2c_bus_control(struct rt_i2c_bus_device *bus, int cmd, void *args)
{
    drv_i2c_cfg_t *cfg            = (drv_i2c_cfg_t *)bus->priv;
    nrfx_twim_t const *p_instance = &cfg->twi_instance;
    switch (cmd) {
        case I2C_RECOVER:
            nrfx_twi_twim_bus_recover(cfg->scl_pin, cfg->sda_pin);
            break;
        case I2C_DISABLE:
            nrfx_twim_uninit(p_instance);
            break;
        default:
            break;
    }
}

static const struct rt_i2c_bus_device_ops _i2c_ops =
    {
        _master_xfer,
        NULL,
        _i2c_bus_control,
};

void i2c0_init(void)
{
    twi_master_init(&i2c0_bus);
}

void i2c0_uninit(void)
{
    nrfx_twim_uninit(&i2c0);
}

int rt_hw_i2c_init(void)
{
#ifdef BSP_USING_I2C0
    i2c0_bus.ops     = &_i2c_ops;
    i2c0_bus.timeout = 0;
    i2c0_bus.priv    = (void *)&drv_i2c_0;
    twi_master_init(&i2c0_bus);
    rt_i2c_bus_device_register(&i2c0_bus, "i2c0");
#endif
#ifdef BSP_USING_I2C1
    i2c1_bus.ops     = &_i2c_ops;
    i2c1_bus.timeout = 0;
    i2c1_bus.priv    = (void *)&drv_i2c_1;
    twi_master_init(&i2c1_bus);
    rt_i2c_bus_device_register(&i2c1_bus, "i2c1");
#endif
#ifdef BSP_USING_I2C2
    i2c2_bus.ops     = &_i2c_ops;
    i2c2_bus.timeout = 0;
    i2c2_bus.priv    = (void *)&drv_i2c_2;
    twi_master_init(&i2c2_bus);
    rt_i2c_bus_device_register(&i2c2_bus, "i2c2");
#endif
#ifdef BSP_USING_I2C3
    i2c3_bus.ops     = &_i2c_ops;
    i2c3_bus.timeout = 0;
    i2c3_bus.priv    = (void *)&drv_i2c_3;
    twi_master_init(&i2c3_bus);
    rt_i2c_bus_device_register(&i2c3_bus, "i2c3");
#endif

    return 0;
}

INIT_BOARD_EXPORT(rt_hw_i2c_init);
#endif /* defined(BSP_USING_I2C0) || defined(BSP_USING_I2C1) */

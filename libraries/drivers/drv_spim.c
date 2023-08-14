/**
 * @file drv_spim.c
 * @author T0n0T (823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-08-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <stdint.h>
#include <string.h>
#include "board.h"
#include "drv_spim.h"

#define DBG_LEVEL DBG_LOG
#include <rtdbg.h>
#define LOG_TAG "drv.spim"

#ifdef BSP_USING_SPI
#if NRFX_SPIM_ENABLED
#if defined(BSP_USING_SPI0) || defined(BSP_USING_SPI1) || defined(BSP_USING_SPI2) || defined(BSP_USING_SPI3) || defined(BSP_USING_SPI4)
static struct nrfx_drv_spim spi_drv[] =
    {
#ifdef BSP_USING_SPI0
        NRFX_SPI0_CONFIG,
#endif

#ifdef BSP_USING_SPI1
        NRFX_SPI1_CONFIG,
#endif

#ifdef BSP_USING_SPI2
        NRFX_SPI2_CONFIG,
#endif

};

/**
 * @brief  This function config spi bus
 * @param  device
 * @param  configuration
 * @retval RT_EOK / -RT_ERROR
 */
static rt_err_t spi_configure(struct rt_spi_device *device, struct rt_spi_configuration *configuration)
{
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);
    RT_ASSERT(configuration != RT_NULL);
    struct nrfx_drv_spim *spim_instance = (struct nrfx_drv_spim *)device->bus->parent.user_data;
    nrfx_spim_t *p_instance             = &(spim_instance->instance);
    nrfx_spim_config_t config           = spim_instance->nrfx_spim_cfg;

    if ((rt_base_t)device->parent.user_data != RT_NULL) {
        config.ss_pin = (rt_base_t)device->parent.user_data;
        LOG_D("spi_bus [%s] configure SS_PIN = %d", device->parent.parent.name, config.ss_pin);
    }

    /* spi config bit order */
    if (configuration->mode & RT_SPI_MSB) {
        config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
        LOG_D("spi_bus [%s] configure MSB_FIRST", device->parent.parent.name);
    } else {
        config.bit_order = NRF_SPIM_BIT_ORDER_LSB_FIRST;
        LOG_D("spi_bus [%s] configure LSB_FIRST", device->parent.parent.name);
    }
    /* spi mode config */
    switch (configuration->mode & RT_SPI_MODE_3) {
        case RT_SPI_MODE_0 /* RT_SPI_CPOL:0 , RT_SPI_CPHA:0 */:
            config.mode = NRF_SPIM_MODE_0;
            break;
        case RT_SPI_MODE_1 /* RT_SPI_CPOL:0 , RT_SPI_CPHA:1 */:
            config.mode = NRF_SPIM_MODE_1;
            break;
        case RT_SPI_MODE_2 /* RT_SPI_CPOL:1 , RT_SPI_CPHA:0 */:
            config.mode = NRF_SPIM_MODE_2;
            break;
        case RT_SPI_MODE_3 /* RT_SPI_CPOL:1 , RT_SPI_CPHA:1 */:
            config.mode = NRF_SPIM_MODE_3;
            break;
        default:
            LOG_E("spi_configure mode error %x\n", configuration->mode);
            return -RT_ERROR;
    }
    LOG_D("spi_bus [%s] configure SPI_MODE_%d", device->parent.parent.name, config.mode);
    /* spi frequency config */
    switch (configuration->max_hz / 1000) {
        case 125:
            config.frequency = NRF_SPIM_FREQ_125K;
            break;
        case 250:
            config.frequency = NRF_SPIM_FREQ_250K;
            break;
        case 500:
            config.frequency = NRF_SPIM_FREQ_500K;
            break;
        case 1000:
            config.frequency = NRF_SPIM_FREQ_1M;
            break;
        case 2000:
            config.frequency = NRF_SPIM_FREQ_2M;
            break;
        case 4000:
            config.frequency = NRF_SPIM_FREQ_4M;
            break;
        case 8000:
            config.frequency = NRF_SPIM_FREQ_8M;
            break;
        default:
            LOG_E("spi_configure rate error %d\n", configuration->max_hz);
            break;
    }
    LOG_D("spi_bus [%s] configure SPI_FREQ = %u", device->parent.parent.name, config.frequency);

    nrfx_err_t result = nrfx_spim_init(p_instance, &config, RT_NULL, (void *)(p_instance->drv_inst_idx));
    if ((NRFX_SUCCESS != result)) {
        LOG_E("spi_bus [%s] init failed err[%d]", device->parent.parent.name, result);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_uint32_t spixfer(struct rt_spi_device *device, struct rt_spi_message *message)
{
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);

    nrfx_err_t nrf_ret;
    struct nrfx_drv_spim *spim_instance = (struct nrfx_drv_spim *)device->bus->parent.user_data;
    nrfx_spim_t *p_instance             = &(spim_instance->instance);

    nrfx_spim_xfer_desc_t spim_xfer_desc =
        {
            .p_tx_buffer = message->send_buf,
            .tx_length   = message->length,
            .p_rx_buffer = message->recv_buf,
            .rx_length   = message->length,
        };
    // printf("\nspim send: \n");
    // for (size_t i = 0; i < spim_xfer_desc.tx_length; i++) {
    //     printf("%02x ", *(spim_xfer_desc.p_tx_buffer + i));
    // }
    nrf_ret = nrfx_spim_xfer(p_instance, &spim_xfer_desc, 0);

    // printf("\nspim recv: \n");
    // for (size_t i = 0; i < spim_xfer_desc.rx_length; i++) {
    //     printf("%02x ", *(spim_xfer_desc.p_rx_buffer + i));
    // }
    if (NRFX_SUCCESS != nrf_ret) {
        return 0;
    } else {
        return message->length;
    }
}

/* spi bus callback function  */
static const struct rt_spi_ops nrfx_spi_ops =
    {
        .configure = spi_configure,
        .xfer      = spixfer,
};

/*spi bus init*/
static int rt_hw_spi_bus_init(void)
{
    for (int i = 0; i < sizeof(spi_drv) / sizeof(spi_drv[0]); i++) {
        spi_drv[i].spi_bus.parent.user_data = &spi_drv[i];
        if (RT_EOK != rt_spi_bus_register(&spi_drv[i].spi_bus, spi_drv[i].bus_name, &nrfx_spi_ops)) {
            LOG_E("spi_bus [%s] register failed", spi_drv[i].bus_name);
            return -RT_ERROR;
        }
    }
    return RT_EOK;
}

int rt_hw_spi_init(void)
{
    return rt_hw_spi_bus_init();
}
INIT_BOARD_EXPORT(rt_hw_spi_init);

/**
 * Attach the spi device to SPI bus, this function must be used after initialization.
 */
rt_err_t rt_hw_spi_device_attach(const char *bus_name, const char *device_name, rt_base_t ss_pin)
{
    RT_ASSERT(bus_name != RT_NULL);
    RT_ASSERT(device_name != RT_NULL);
    RT_ASSERT(ss_pin != RT_NULL);

    rt_err_t result;
    struct rt_spi_device *spi_device;
    /* attach the device to spi bus*/
    spi_device = (struct rt_spi_device *)rt_malloc(sizeof(struct rt_spi_device));
    RT_ASSERT(spi_device != RT_NULL);
    /* initialize the cs pin */
    result = rt_spi_bus_attach_device(spi_device, device_name, bus_name, (void *)ss_pin);
    if (result != RT_EOK) {
        LOG_E("%s attach to %s faild, %d", device_name, bus_name, result);
        result = -RT_ERROR;
    }
    RT_ASSERT(result == RT_EOK);
    return result;
}

#endif
#endif
#endif

/**
 * @file drv_spim.h
 * @author T0n0T (823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-08-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>

#ifndef __DRV_SPI_H_
#define __DRV_SPI_H_

#ifdef BSP_USING_SPI
#if NRFX_SPIM_ENABLED
#include "nrfx_spim.h"

/**
 * @brief Attach the spi device to SPI bus, this function must be used after initialization.
 * @param bus_name     spi bus name  "spi0"/"spi1"/"spi2"
 * @param device_name  spi device name "spi0x"/"spi1x"/"spi2x"
 * @param ss_pin       spi ss pin number
 * @retval  -RT_ERROR / RT_EOK
 */
rt_err_t rt_hw_spi_device_attach(const char *bus_name, const char *device_name, rt_base_t ss_pin);

// SPI bus config
#ifdef BSP_USING_SPI0
#define NRFX_SPI0_CONFIG                                                   \
    {                                                                      \
        .bus_name      = "spi0",                                           \
        .instance      = NRFX_SPIM_INSTANCE(0),                            \
        .nrfx_spim_cfg = NRFX_SPIM_DEFAULT_CONFIG(BSP_SPI0_SCK_PIN,        \
                                                  BSP_SPI0_MOSI_PIN,       \
                                                  BSP_SPI0_MISO_PIN,       \
                                                  NRFX_SPIM_PIN_NOT_USED), \
    }
#endif
#ifdef BSP_USING_SPI1
#define NRFX_SPI1_CONFIG                                                   \
    {                                                                      \
        .bus_name      = "spi1",                                           \
        .instance      = NRFX_SPIM_INSTANCE(1),                            \
        .nrfx_spim_cfg = NRFX_SPIM_DEFAULT_CONFIG(BSP_SPI1_SCK_PIN,        \
                                                  BSP_SPI1_MOSI_PIN,       \
                                                  BSP_SPI1_MISO_PIN,       \
                                                  NRFX_SPIM_PIN_NOT_USED), \
    }
#endif

#ifdef BSP_USING_SPI2
#define NRFX_SPI2_CONFIG                                                   \
    {                                                                      \
        .bus_name      = "spi2",                                           \
        .instance      = NRFX_SPIM_INSTANCE(2),                            \
        .nrfx_spim_cfg = NRFX_SPIM_DEFAULT_CONFIG(BSP_SPI2_SCK_PIN,        \
                                                  BSP_SPI2_MOSI_PIN,       \
                                                  BSP_SPI2_MISO_PIN,       \
                                                  NRFX_SPIM_PIN_NOT_USED), \
    }
#endif

struct nrfx_drv_spim {
    char *bus_name;
    nrfx_spim_t instance;             /* nrfx spi driver instance. */
    nrfx_spim_config_t nrfx_spim_cfg; /* nrfx spi config Configuration */
    struct rt_spi_configuration *cfg;
    struct rt_spi_bus spi_bus;
};

#else
#include "nrfx_spi.h"

/**
 * @brief Attach the spi device to SPI bus, this function must be used after initialization.
 * @param bus_name     spi bus name  "spi0"/"spi1"/"spi2"
 * @param device_name  spi device name "spi0x"/"spi1x"/"spi2x"
 * @param ss_pin       spi ss pin number
 * @retval  -RT_ERROR / RT_EOK
 */
rt_err_t rt_hw_spi_device_attach(const char *bus_name, const char *device_name, rt_uint32_t ss_pin);

// SPI bus config
#ifdef BSP_USING_SPI0
#define NRFX_SPI0_CONFIG                 \
    {                                    \
        .bus_name = "spi0",              \
        .spi      = NRFX_SPI_INSTANCE(0) \
    }
#endif
#ifdef BSP_USING_SPI1
#define NRFX_SPI1_CONFIG                 \
    {                                    \
        .bus_name = "spi1",              \
        .spi      = NRFX_SPI_INSTANCE(1) \
    }
#endif

#ifdef BSP_USING_SPI2
#define NRFX_SPI2_CONFIG                 \
    {                                    \
        .bus_name = "spi2",              \
        .spi      = NRFX_SPI_INSTANCE(2) \
    }
#endif

struct nrfx_drv_spi_config {
    char *bus_name;
    nrfx_spi_t spi;
};

struct nrfx_drv_spi {
    nrfx_spi_t spi;               /* nrfx spi driver instance. */
    nrfx_spi_config_t spi_config; /* nrfx spi config Configuration */
    struct rt_spi_configuration *cfg;
    struct rt_spi_bus spi_bus;
};

struct nrfx_drv_spi_pin_config {
    rt_uint8_t sck_pin;
    rt_uint8_t mosi_pin;
    rt_uint8_t miso_pin;
    rt_uint8_t ss_pin;
};
#endif
#endif /* BSP_USING_SPI */
#endif /*__DRV_SPI_H_*/

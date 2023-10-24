/**
 * @file ccm3310.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-07-24
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "stdio.h"
#include <string.h>
#include <ccm3310.h>

nrfx_spim_t instance           = NRFX_SPIM_INSTANCE(1);
nrfx_spim_config_t config_spim = NRFX_SPIM_DEFAULT_CONFIG;
uint8_t recv_buf[1024];

int ccm3310_init(void)
{
    config_spim.sck_pin   = PIN_SCK;
    config_spim.mosi_pin  = PIN_MOSI;
    config_spim.miso_pin  = PIN_MISO;
    config_spim.ss_pin    = PIN_SS;
    config_spim.frequency = NRF_SPIM_FREQ_1M;
    config_spim.mode      = NRF_SPIM_MODE_3;
    nrfx_spim_init(&instance, &config_spim, 0, (void *)instance.drv_inst_idx);
    return 0;
}

void ccm3310_uninit(void)
{
    nrfx_spim_uninit(&instance);
}

int ccm3310_transfer(uint8_t *send_buf, int send_len, uint8_t **decode_data, int recv_len)
{
    int len           = 0;
    uint8_t status    = 1;
    nrfx_err_t result = NRFX_SUCCESS;

    memset(recv_buf, 0xff, sizeof(recv_buf));
    // nrfx_spim_init(&instance, &config_spim, 0, (void *)instance.drv_inst_idx);
    nrf_gpio_pin_write(GINT0, 0);
    while (status == 1) {
        status = nrf_gpio_pin_read(GINT1);
    }

    nrfx_spim_xfer_desc_t spim_xfer_desc =
        {
            .p_tx_buffer = send_buf,
            .tx_length   = send_len,
            .p_rx_buffer = 0,
            .rx_length   = send_len,
        };
    result = nrfx_spim_xfer(&instance, &spim_xfer_desc, 0);

#if defined(CCM3310_RAW_PRINTF)
    printf("\n========= print transmit ========\n");
    for (size_t i = 0; i < send_len; i++) {
        printf("0x%02x ", *(send_buf + i));
        if (i % 4 == 3) {
            printf("\n");
        }
    }
#else

#endif
    status = 1;
    // rt_pin_write(GINT0, PIN_LOW);
    while (status == 1) {
        status = nrf_gpio_pin_read(GINT1);
    }

    spim_xfer_desc.p_tx_buffer = 0;
    spim_xfer_desc.tx_length   = recv_len;
    spim_xfer_desc.p_rx_buffer = recv_buf;
    spim_xfer_desc.rx_length   = recv_len;

    result = nrfx_spim_xfer(&instance, &spim_xfer_desc, 0);
    // nrfx_spim_uninit(&instance);
#if defined(CCM3310_RAW_PRINTF)
    printf("\n========= print receive =========\n");
    for (size_t i = 0; i < recv_len; i++) {
        printf("0x%02x ", recv_buf[i]);
        if (i % 4 == 3) {
            printf("\n");
        }
        if (recv_buf[i] == 0x01 && recv_buf[i - 1] == 0x33 && recv_buf[i - 2] == 0x02 && recv_buf[i - 3] == 0x56) {
            break;
        }
    }
#endif
    int err = decode(recv_buf, decode_data, &len);
    if (err == 0) {
        return len;
    }
    return -1;
}

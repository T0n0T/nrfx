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

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <ccm3310.h>
#include "nrfx_spim.h"

struct rt_spi_device ccm;
nrfx_spim_t instance = NRFX_SPIM_INSTANCE(1);
uint8_t recv_buf[1024];

void ccm3310_init(void)
{
    rt_pin_mode(POR, PIN_MODE_OUTPUT);
    rt_pin_mode(GINT0, PIN_MODE_OUTPUT);
    // rt_pin_mode(15, PIN_MODE_OUTPUT);
    rt_pin_mode(GINT1, PIN_MODE_INPUT);
    nrfx_spim_config_t config_spim = NRFX_SPIM_DEFAULT_CONFIG(PIN_SCK, PIN_MOSI, PIN_MISO, PIN_SS);
    config_spim.frequency          = NRF_SPIM_FREQ_1M;
    config_spim.mode               = NRF_SPIM_MODE_3;
    config_spim.miso_pull          = NRF_GPIO_PIN_PULLUP;
    nrfx_spim_init(&instance, &config_spim, 0, (void *)instance.drv_inst_idx);

    rt_pin_write(POR, PIN_LOW);
    rt_thread_mdelay(20);
    rt_pin_write(POR, PIN_HIGH);
}
INIT_APP_EXPORT(ccm3310_init);

int ccm3310_transfer(uint8_t *send_buf, int send_len, uint8_t **decode_data, int recv_len)
{
    int len           = 0;
    rt_int8_t status  = PIN_HIGH;
    nrfx_err_t result = NRFX_SUCCESS;

    rt_memset(recv_buf, 0xff, sizeof(recv_buf));
    rt_pin_write(GINT0, PIN_LOW);
    while (status == PIN_HIGH) {
        status = rt_pin_read(GINT1);
        printf("status: %d", status);
    }

    nrfx_spim_xfer_desc_t spim_xfer_desc =
        {
            .p_tx_buffer = send_buf,
            .tx_length   = send_len,
            .p_rx_buffer = 0,
            .rx_length   = send_len,
        };
    result = nrfx_spim_xfer(&instance, &spim_xfer_desc, 0);
    printf("\nspim transmit:[%x]\n", result);

    printf("\n========= print transmit ========\n");
    for (size_t i = 0; i < send_len; i++) {
        printf("0x%02x ", *(send_buf + i));
        if (i % 4 == 3) {
            printf("\n");
        }
    }
    while (status == PIN_HIGH) {
        status = rt_pin_read(GINT1);
    }

    spim_xfer_desc.p_tx_buffer = 0;
    spim_xfer_desc.tx_length   = recv_len;
    spim_xfer_desc.p_rx_buffer = recv_buf;
    spim_xfer_desc.rx_length   = recv_len;

    result = nrfx_spim_xfer(&instance, &spim_xfer_desc, 0);
    printf("\nspim receive:[%x]\n", result);
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
    int err = decode(recv_buf, decode_data, &len);
    if (err == 0) {
        return len;
    }
    return -1;
}

void ccm3310_sm4_init(uint8_t *key)
{
    uint8_t *pack                 = (uint8_t *)rt_malloc(100);
    struct ccm3310_key_data *data = rt_malloc(sizeof(struct ccm3310_key_data));
    data->version                 = 0x00;
    data->key_id                  = 0x00;
    data->algo                    = 0x84;
    data->len                     = 0x10;
    for (size_t i = 0; i < 16; i++) {
        data->key_data[i] = *(key + i);
    }

    uint8_t *id  = 0;
    int send_len = 0;
    send_len     = encode(0x80, 0x42, 0x00, 0x00, pack, (uint8_t *)data, sizeof(data));
    ccm3310_transfer(pack, send_len, &id, 21);
}

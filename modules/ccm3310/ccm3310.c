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
// #include <drv_spim.h>

#define CCM3310_DEBUG
#if defined(CCM3310_DEBUG)
#define CCM3310_RAW_PRINTF
#define DBG_LVL DBG_LOG
#else
#define DBG_LVL DBG_INFO
#endif

#define DBG_TAG "ccm3310.dev"
#include <rtdbg.h>

struct rt_spi_device ccm;

uint8_t recv_buf[1024];

void ccm3310_init(void)
{
    rt_pin_mode(POR, PIN_MODE_OUTPUT);
    rt_pin_mode(GINT0, PIN_MODE_OUTPUT);
    rt_pin_mode(GINT1, PIN_MODE_INPUT);

    if (rt_spi_bus_attach_device(&ccm, "ccm", "spi1", (void *)CS_PIN) != RT_EOK) {
        LOG_E("Fail to attach %s creating spi_device %s failed.", "spi1", "ccm");
        return;
    }

    struct rt_spi_configuration cfg;
    cfg.data_width = 8;
    cfg.mode       = RT_SPI_MASTER | RT_SPI_MODE_3 | RT_SPI_MSB;
    cfg.max_hz     = 1 * 1000 * 1000;
    rt_spi_configure(&ccm, &cfg);
    rt_pin_write(POR, PIN_LOW);
    rt_thread_mdelay(20);
    rt_pin_write(POR, PIN_HIGH);
}
INIT_APP_EXPORT(ccm3310_init);

/**
 * @brief 模块交互low-level
 *
 * @param send_buf 下行数据包
 * @param send_len 下行包长度
 * @param decode_data 解包上行包数据区，并给出的指针
 * @param recv_len
 * @return int
 */
int ccm3310_transfer(uint8_t *send_buf, int send_len, uint8_t **decode_data, int recv_len)
{
    struct rt_spi_message msg;
    int len          = 0;
    rt_int8_t status = PIN_HIGH;

    rt_memset(recv_buf, 0xff, sizeof(recv_buf));
    rt_pin_write(GINT0, PIN_LOW);
    while (status == PIN_HIGH) {
        status = rt_pin_read(GINT1);
        LOG_D("status: %d", status);
    }

    msg.send_buf   = send_buf;
    msg.recv_buf   = RT_NULL;
    msg.length     = send_len;
    msg.cs_take    = 1;
    msg.cs_release = 1;
    msg.next       = RT_NULL;
    rt_spi_transfer_message(&ccm, &msg);

#if defined(CCM3310_RAW_PRINTF)
    printf("\n========= print transmit ========\n");
    for (size_t i = 0; i < send_len; i++) {
        printf("0x%02x ", *(send_buf + i));
        if (i % 4 == 3) {
            printf("\n");
        }
    }
#endif
    while (status == PIN_HIGH) {
        status = rt_pin_read(GINT1);
    }

    msg.send_buf   = RT_NULL;
    msg.recv_buf   = recv_buf;
    msg.length     = recv_len;
    msg.cs_take    = 1;
    msg.cs_release = 1;
    msg.next       = RT_NULL;
    rt_spi_transfer_message(&ccm, &msg);

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

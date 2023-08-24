/**
 * @file ccm3310.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-07-24
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "drv_gpio.h"
#include "drv_spi.h"
#include <ccm3310_sm4.h>

#define CS_PIN   BSP_SPI1_SS_PIN

#define POR      18
#define GINT0    19
#define GINT1    20

#define PIN_SCK  BSP_SPI1_SCK_PIN
#define PIN_MOSI BSP_SPI1_MOSI_PIN
#define PIN_MISO BSP_SPI1_MISO_PIN
#define PIN_SS   BSP_SPI1_SS_PIN

extern void ccm3310_init(void);
extern void ccm3310_thread_start(void);

int ccm3310_transfer(uint8_t *send_buf, int send_len, uint8_t **decode_data, int recv_len);

int numalgin(int num, int align);
int decode(uint8_t *raw, uint8_t **data, int *len);
int encode(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, uint8_t *send_pack, uint8_t *data, uint32_t data_len);

uint32_t crc32(const uint8_t *buf, uint32_t size);

/**
 * @file custom_board.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-23
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "nrf_gpio.h"

#define LEDS_NUMBER       3

#define LED_START         25
#define LED_1             25
#define LED_2             26
#define LED_3             27
#define LED_STOP          27

#define LEDS_ACTIVE_STATE 0

#define LEDS_INV_MASK     LEDS_MASK

#define LEDS_LIST           \
    {                       \
        LED_1, LED_2, LED_3 \
    }

#define BSP_LED_0            LED_1
#define BSP_LED_1            LED_2
#define BSP_LED_2            LED_3

#define BUTTONS_NUMBER       1

#define BUTTON_START         19
#define BUTTON_1             19
#define BUTTON_STOP          19
#define BUTTON_PULL          NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST \
    {                \
        BUTTON_1     \
    }

#define BSP_BUTTON_0   BUTTON_1

#define RX_PIN_NUMBER  4
#define TX_PIN_NUMBER  5
#define HWFC           false

#define SPIM0_SCK_PIN  16 // SPI clock GPIO pin number.
#define SPIM0_MOSI_PIN 13 // SPI Master Out Slave In GPIO pin number.
#define SPIM0_MISO_PIN 14 // SPI Master In Slave Out GPIO pin number.
#define SPIM0_SS_PIN   15 // SPI Slave Select GPIO pin number.
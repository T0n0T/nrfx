#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <rtconfig.h>

#define NRF_DFU_TRANSPORT_BLE 1
#define NRFX_SYSTICK_ENABLED  1
#define NRFX_NVMC_ENABLED     1
#define BLE_BAS_ENABLED       1
#define BLE_DIS_ENABLED       1
#define BLE_HRS_ENABLED       1

#if 0 
// ccm3310s-t
#define CCM_POR         -1
#define CCM_GINT0       11
#define CCM_GINT1       12
#define CCM_PIN_SCK     16
#define CCM_PIN_MOSI    13
#define CCM_PIN_MISO    14
#define CCM_PIN_SS      15

// max30102
#define MAX_PIN_INT     29
#define MAX_PIN_SCL     30
#define MAX_PIN_SDA     31

// hal
#define BEEP            2
#define SW              19
#define LED1            25
#define LED2            26
#define LED3            27
#define POWER_KEEP      28

// ec800m
#define UART_PIN_RX     4
#define UART_PIN_TX     5
#define EC800_PIN_RESET 22
#define EC800_PIN_PWR   3

#else

// ccm3310s-t
#define CCM_POR      -1
#define CCM_GINT0    28
#define CCM_GINT1    3
#define CCM_PIN_SCK  13
#define CCM_PIN_MOSI 15
#define CCM_PIN_MISO 16
#define CCM_PIN_SS   14

// max30102
#define MAX_PIN_INT 4
#define MAX_PIN_SCL 5
#define MAX_PIN_SDA 7

// hal
#define BEEP       12
#define SW         28
#define LED1       17
#define LED2       18
#define LED3       19
#define POWER_KEEP -1

// ec800m
#define UART_PIN_RX     8
#define UART_PIN_TX     6
#define EC800_PIN_RESET -1
#define EC800_PIN_PWR   -1
#endif

#endif // APP_CONFIG_H

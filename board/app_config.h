#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define NRF_LOG_DEFERRED              0
#define NRFX_SYSTICK_ENABLED          1
#define NRFX_NVMC_ENABLED             1
#define NRF_PWR_MGMT_ENABLED          1
#define NRF_DFU_TRANSPORT_BLE         1
#define APP_UART_ENABLED              1
#define APP_FIFO_ENABLED              1
#define APP_UART_DRIVER_INSTANCE      0
#define BLE_BAS_ENABLED               1
#define BLE_DIS_ENABLED               1
#define BLE_HRS_ENABLED               1
#define BLE_LOG_ENABLED               1
#define SM4_ENABLED                   1

#define EC800M_MQTT_SOFT              0
#define MQTT_PUBLISH_DEFAULT_INTERVAL 5000

#define EC800M_MQTT_DEFAULT_CFG                             \
    {                                                       \
        .host      = "172.16.248.66",                       \
        .port      = "18891",                               \
        .keepalive = 300,                                   \
        .clientid  = "CYG_5E89460B99",                      \
        .username  = NULL,                                  \
        .password  = NULL,                                  \
        .subtopic  = "/iot/CYG_5E89460B99/BR/device/reply", \
        .pubtopic  = "/iot/CYG_5E89460B99/BR/device/data",  \
    }

// #define EC800M_MQTT_DEFAULT_CFG                        \
//     {                                                  \
//         .host      = "112.125.89.8",                   \
//         .port      = "47369",                          \
//         .keepalive = 300,                              \
//         .clientid  = "CYGC_TEST",                      \
//         .username  = NULL,                             \
//         .password  = NULL,                             \
//         .subtopic  = "/iot/CYGC_TEST/BR/device/reply", \
//         .pubtopic  = "/iot/CYGC_TEST/BR/device/data",  \
//     }

#define SM4_DEFAULT_KEY                                                                                \
    {                                                                                                  \
        0x77, 0x7f, 0x23, 0xc6, 0xfe, 0x7b, 0x48, 0x73, 0xdd, 0x59, 0x5c, 0xff, 0xf6, 0x5f, 0x58, 0xec \
    }

#define configPRE_SLEEP_PROCESSING \
    do {                           \
        extern void PRE_SLEEP();   \
        PRE_SLEEP();               \
    } while (0);
#define configPOST_SLEEP_PROCESSING \
    do {                            \
        extern void POST_SLEEP();   \
        POST_SLEEP();               \
    } while (0);

#if 1
// ccm3310s-t
#define CCM_POR      -1
#define CCM_GINT0    11
#define CCM_GINT1    12
#define CCM_PIN_SCK  16
#define CCM_PIN_MOSI 13
#define CCM_PIN_MISO 14
#define CCM_PIN_SS   15

// max30102
#define MAX_PIN_INT 29
#define MAX_PIN_SCL 30
#define MAX_PIN_SDA 31

// hal
#define BEEP       2
#define SW         19
#define LED1       25
#define LED2       26
#define LED3       27
#define POWER_KEEP 28

// ec800m
#define UART_PIN_RX     4
#define UART_PIN_TX     5
#define EC800_PIN_RESET 22
#define EC800_PIN_PWR   3
#define EC800_PIN_DTR   7
#define EC800_PIN_RI    6
#define EC800_PWR_EN    8

#else

// ccm3310s-t
#define CCM_POR         -1
#define CCM_GINT0       28
#define CCM_GINT1       3
#define CCM_PIN_SCK     13
#define CCM_PIN_MOSI    15
#define CCM_PIN_MISO    16
#define CCM_PIN_SS      14

// max30102
#define MAX_PIN_INT     4
#define MAX_PIN_SCL     5
#define MAX_PIN_SDA     7

// hal
#define BEEP            -1
#define SW              20
#define LED1            17
#define LED2            18
#define LED3            19
#define POWER_KEEP      -1

// ec800m
#define UART_PIN_RX     8
#define UART_PIN_TX     6
#define EC800_PIN_RESET -1
#define EC800_PIN_PWR   -1
#define EC800_PIN_DTR   12
#define EC800_PIN_RI    -1
#endif

#endif // APP_CONFIG_H

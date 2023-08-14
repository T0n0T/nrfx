#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

/* Automatically generated file; DO NOT EDIT. */
/* RT-Thread Configuration */

/* RT-Thread Kernel */

#define RT_NAME_MAX   8
#define RT_ALIGN_SIZE 8
#define RT_THREAD_PRIORITY_32
#define RT_THREAD_PRIORITY_MAX 32
#define RT_TICK_PER_SECOND     100
#define RT_USING_OVERFLOW_CHECK
#define RT_USING_HOOK
#define RT_HOOK_USING_FUNC_PTR
#define RT_USING_IDLE_HOOK
#define RT_IDLE_HOOK_LIST_SIZE 4
#define IDLE_THREAD_STACK_SIZE 256
#define RT_USING_TIMER_SOFT
#define RT_TIMER_THREAD_PRIO       4
#define RT_TIMER_THREAD_STACK_SIZE 512

/* kservice optimization */

#define RT_USING_DEBUG
#define RT_DEBUGING_COLOR
#define RT_DEBUGING_CONTEXT
#define RT_DEBUGING_INIT

/* Inter-Thread communication */

#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MAILBOX
#define RT_USING_MESSAGEQUEUE

/* Memory Management */

#define RT_USING_MEMPOOL
#define RT_USING_SMALL_MEM
#define RT_USING_SMALL_MEM_AS_HEAP
#define RT_USING_HEAP

/* Kernel Device Object */

#define RT_USING_DEVICE
#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE     128
#define RT_CONSOLE_DEVICE_NAME "uart0"
#define RT_VER_NUM             0x50001

/* RT-Thread Architecture */

/* RT-Thread Components */

#define RT_USING_COMPONENTS_INIT
#define RT_USING_USER_MAIN
#define RT_MAIN_THREAD_STACK_SIZE 2048
#define RT_MAIN_THREAD_PRIORITY   10
#define RT_USING_LEGACY
#define RT_USING_MSH
#define RT_USING_FINSH
#define FINSH_USING_MSH
#define FINSH_THREAD_NAME       "tshell"
#define FINSH_THREAD_PRIORITY   20
#define FINSH_THREAD_STACK_SIZE 4096
#define FINSH_USING_HISTORY
#define FINSH_HISTORY_LINES 5
#define FINSH_USING_SYMTAB
#define FINSH_CMD_SIZE 80
#define MSH_USING_BUILT_IN_COMMANDS
#define FINSH_USING_DESCRIPTION
#define FINSH_ARG_MAX 10

/* DFS: device virtual file system */

#define RT_USING_DFS
#define DFS_USING_POSIX
#define DFS_USING_WORKDIR
#define DFS_FD_MAX 16
#define RT_USING_DFS_V1
#define DFS_FILESYSTEMS_MAX      4
#define DFS_FILESYSTEM_TYPES_MAX 4
#define RT_USING_DFS_DEVFS

/* Device Drivers */

#define RT_USING_DEVICE_IPC
#define RT_UNAMED_PIPE_NUMBER 64
#define RT_USING_SYSTEM_WORKQUEUE
#define RT_SYSTEM_WORKQUEUE_STACKSIZE 2048
#define RT_SYSTEM_WORKQUEUE_PRIORITY  23
#define RT_USING_SERIAL
#define RT_USING_SERIAL_V1
#define RT_SERIAL_USING_DMA
#define RT_SERIAL_RB_BUFSZ 256
#define RT_USING_I2C
#define RT_USING_I2C_BITOPS
#define RT_USING_PIN
#define RT_USING_RTC
#define RT_USING_SPI
#define RT_USING_SENSOR
#define RT_USING_SENSOR_CMD

/* Using USB */

/* C/C++ and POSIX layer */

#define RT_LIBC_DEFAULT_TIMEZONE 8

/* POSIX (Portable Operating System Interface) layer */

#define RT_USING_POSIX_FS
#define RT_USING_POSIX_DEVIO
#define RT_USING_POSIX_POLL
#define RT_USING_POSIX_SELECT
#define RT_USING_POSIX_SOCKET

/* Interprocess Communication (IPC) */

#define RT_USING_POSIX_PIPE
#define RT_USING_POSIX_PIPE_SIZE 512

/* Socket is in the 'Network' category */

/* Network */

#define RT_USING_SAL
#define SAL_INTERNET_CHECK

/* Docking with protocol stacks */

#define SAL_USING_AT
#define SAL_USING_POSIX
#define RT_USING_NETDEV
#define NETDEV_USING_IFCONFIG
#define NETDEV_USING_PING
#define NETDEV_USING_NETSTAT
#define NETDEV_USING_AUTO_DEFAULT
#define NETDEV_IPV4 1
#define NETDEV_IPV6 0
#define RT_USING_AT
#define AT_USING_CLIENT
#define AT_CLIENT_NUM_MAX 1
#define AT_USING_SOCKET
#define AT_USING_CLI
#define AT_CMD_MAX_LEN    128
#define AT_SW_VERSION_NUM 0x10301

/* Utilities */

#define RT_USING_RESOURCE_ID

/* RT-Thread online packages */

/* IoT - internet of things */

#define PKG_USING_PAHOMQTT
#define PAHOMQTT_PIPE_MODE
#define RT_PKG_MQTT_THREAD_STACK_SIZE   4096
#define PKG_PAHOMQTT_SUBSCRIBE_HANDLERS 1
#define MQTT_DEBUG
#define PKG_USING_PAHOMQTT_LATEST

/* Wi-Fi */

/* Marvell WiFi */

/* Wiced WiFi */

#define PKG_USING_AT_DEVICE
#define PKG_USING_AT_DEVICE_LATEST_VERSION
#define PKG_AT_DEVICE_VER_NUM 0x99999
#define PKG_USING_WIZNET
#define WIZ_USING_W5500

/* WIZnet device configure */
#define WIZ_DEBUG
#define WIZ_SPI_DEVICE "w5500"
#define WIZ_RST_PIN    3
#define WIZ_IRQ_PIN    12
#define WIZ_USING_DHCP
#define WIZ_USING_PING
#define PKG_USING_WIZNET_LATEST_VERSION

/* IoT Cloud */

/* security packages */

/* language packages */

/* JSON: JavaScript Object Notation, a lightweight data-interchange format */

/* XML: Extensible Markup Language */

/* multimedia packages */

/* LVGL: powerful and easy-to-use embedded GUI library */

/* u8g2: a monochrome graphic library */

/* tools packages */

#define PKG_USING_CMBACKTRACE
#define PKG_CMBACKTRACE_PLATFORM_M4
#define PKG_CMBACKTRACE_DUMP_STACK
#define PKG_CMBACKTRACE_PRINT_ENGLISH
#define PKG_USING_CMBACKTRACE_V10401
#define PKG_CMBACKTRACE_VER_NUM 0x10401
#define PKG_USING_SEGGER_RTT
#define SEGGER_RTT_ENABLE
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   3
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 3
#define BUFFER_SIZE_UP                  1024
#define BUFFER_SIZE_DOWN                16
#define SEGGER_RTT_PRINTF_BUFFER_SIZE   64
#define RTT_DEFAULT_BUFFER_INDEX        0
#define RTT_DEFAULT_TERMINAL_INDEX      0
#define PKG_USING_SEGGER_RTT_LATEST_VERSION
#define PKG_USING_GPS_RMC
#define PKG_USING_GPS_RMC_LATEST_VERSION

/* system packages */

/* enhanced kernel services */

/* acceleration: Assembly language or algorithmic acceleration packages */

/* CMSIS: ARM Cortex-M Microcontroller Software Interface Standard */

/* Micrium: Micrium software products porting for RT-Thread */

/* peripheral libraries and drivers */

/* sensors drivers */

/* touch drivers */

#define NRFX_RTC_ENABLED  1
#define NRFX_RTC1_ENABLED 1
#define PKG_USING_NRFX
#define PKG_USING_NRFX_LATEST_VERSION

/* Kendryte SDK */

/* AI packages */

/* Signal Processing and Control Algorithm Packages */

/* miscellaneous packages */

/* project laboratory */

/* samples: kernel and components samples */

/* entertainment: terminal games and other interesting software packages */

/* Arduino libraries */

/* Projects and Demos */

/* Sensors */

/* Display */

/* Timing */

/* Data Processing */

/* Data Storage */

/* Communication */

/* Device Control */

/* Other */

/* Signal IO */

/* Uncategorized */

/* Hardware Drivers Config */

#define SOC_NRF52832
#define NRFX_CLOCK_ENABLED                     1
#define NRFX_CLOCK_DEFAULT_CONFIG_IRQ_PRIORITY 7
#define NRFX_CLOCK_CONFIG_LF_SRC               1
#define SOC_NORDIC

/* Onboard Peripheral Drivers */

#define BSP_USING_JLINK_TO_USART

/* On-chip Peripheral Drivers */

#define BSP_USING_GPIO
#define NRFX_GPIOTE_ENABLED 1

// #define BSP_USING_UART
// #define NRFX_USING_UART
// #define BSP_USING_UART0
// #define BSP_UART0_RX_PIN   12
// #define BSP_UART0_TX_PIN   3
// #define NRFX_UART_ENABLED  1
// #define NRFX_UART0_ENABLED 1

#define BSP_USING_UARTE
#define NRFX_USING_UARTE
#define BSP_USING_UART0
#define BSP_UART0_RX_PIN 8
#define BSP_UART0_TX_PIN 6
// #define BSP_UART0_RX_PIN    12
// #define BSP_UART0_TX_PIN    3
#define NRFX_UARTE_ENABLED  1
#define NRFX_UARTE0_ENABLED 1

#define BSP_USING_I2C
#define NRFX_TWIM_ENABLED 1
#define BSP_USING_I2C0
#define NRFX_TWIM0_ENABLED 1
#define BSP_I2C0_SCL_PIN   5
#define BSP_I2C0_SDA_PIN   7
#define BSP_USING_SPI
#define NRFX_SPIM_ENABLED 1
// #define BSP_USING_SPI1
#define NRFX_SPIM1_ENABLED 1
#define BSP_SPI1_SCK_PIN   13
#define BSP_SPI1_MOSI_PIN  16
#define BSP_SPI1_MISO_PIN  14
#define BSP_SPI1_SS_PIN    15
#define BSP_USING_SPI2
#define NRFX_SPIM2_ENABLED 1
#define BSP_SPI2_SCK_PIN   13
#define BSP_SPI2_MOSI_PIN  15
#define BSP_SPI2_MISO_PIN  16
#define BSP_SPI2_SS_PIN    14
// #define BSP_SPI2_SCK_PIN   24
// #define BSP_SPI2_MOSI_PIN  22
// #define BSP_SPI2_MISO_PIN  27
// #define BSP_SPI2_SS_PIN    23

/* On-chip flash config */

#define MCU_FLASH_START_ADDRESS 0x00000000
#define MCU_FLASH_SIZE_KB       512
#define MCU_SRAM_START_ADDRESS  0x20000000
#define MCU_SRAM_SIZE_KB        64
#define MCU_FLASH_PAGE_SIZE     0x1000
#define BSP_USING_ONCHIP_RTC
#define NRFX_RTC0_ENABLED 1
#define NRFX_RTC2_ENABLED 1
#define RTC_INSTANCE_ID   2
#define BLE_STACK_USING_NULL

#endif

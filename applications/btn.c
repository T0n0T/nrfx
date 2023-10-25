/**
 * @file btn.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-12
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdio.h>
#include <button.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "bsp.h"
#include "nrfx_gpiote.h"
#include "nrf_pwr_mgmt.h"

#define NRF_LOG_MODULE_NAME btn
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define OS_DELAY vTaskDelay

static Button_t SW_BUTTON;
static TaskHandle_t m_btn_task;
static TimerHandle_t button_tmr;

uint8_t read_sw_btn(void)
{
    return nrf_gpio_pin_read(SW);
}

void btn_release(void)
{
    NRF_LOG_DEBUG("click release!");
    xTimerStart(button_tmr, 0);
}

void btn_click(void)
{
    NRF_LOG_DEBUG("single click!");
}

void btn_double(void)
{
    NRF_LOG_DEBUG("double click!");
}

void btn_long_free(void)
{
    NRF_LOG_DEBUG("long click!");
    beep_on();
    OS_DELAY(500);
    bsp_uninit();
    set_sleep_exit_pin();
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_STAY_IN_SYSOFF);
}

void btn_create(Button_t *p)
{
    Button_Create(
        "SW",
        p,
        read_sw_btn,
        0);
    Button_Attach(p, BUTTON_UP, btn_release);
    Button_Attach(p, BUTTON_DOWM, btn_click);
    Button_Attach(p, BUTTON_DOUBLE, btn_double);
    Button_Attach(p, BUTTON_LONG_FREE, btn_long_free);
}

static void btn_work_on(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    NRF_LOG_DEBUG("btn work on!");
    BaseType_t xHigherPriorityTaskWoken;
    if (pin == SW) {
        xTaskResumeFromISR(m_btn_task);
    }
}

#include "SEGGER_RTT.h"
void btn_work_off(TimerHandle_t xTimer)
{
    __disable_irq();
    vTaskSuspend(m_btn_task);
    __enable_irq();
}

static void btn_task(void *pvParameter)
{
    while (1) {
        Button_Process();
        vTaskDelay(30);
    }
}

void btn_init(void)
{
    nrfx_gpiote_init();
    nrfx_gpiote_in_config_t cfg = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(0);
    cfg.pull                    = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(SW, &cfg, btn_work_on);
    nrfx_gpiote_in_event_enable(SW, true);
    // nrf_gpio_cfg_input(SW, NRF_GPIO_PIN_PULLUP);
    btn_create(&SW_BUTTON);
    button_tmr = xTimerCreate(
        "BtnTimer",
        pdMS_TO_TICKS(5000),
        pdFALSE,
        0,
        btn_work_off);

    BaseType_t xReturned = xTaskCreate(btn_task,
                                       "BLE",
                                       256,
                                       0,
                                       5,
                                       &m_btn_task);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
    vTaskSuspend(m_btn_task);
}
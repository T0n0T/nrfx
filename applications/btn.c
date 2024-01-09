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
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "bsp.h"
#include "nrfx_gpiote.h"
#include "nrf_pwr_mgmt.h"

#define NRF_LOG_MODULE_NAME btn
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define OS_DELAY vTaskDelay

static Button_t      SW_BUTTON;
static TaskHandle_t  m_btn_task;
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

#include "ec800m.h"
void btn_click(void)
{
    NRF_LOG_INFO("single click!");
    extern SemaphoreHandle_t m_app_sem;
    xSemaphoreGive(m_app_sem);
}

void btn_double(void)
{
    NRF_LOG_INFO("double click!");
    beep_on();
    extern uint8_t sm4_flag;
    if (sm4_flag) {
        sm4_flag = 0;
    } else {
        sm4_flag = 1;
    }
}

void btn_long_free(void)
{
    NRF_LOG_INFO("long click!");
    beep_on();
    OS_DELAY(500);
    bsp_uninit();
    set_sleep_exit_pin();
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_STAY_IN_SYSOFF);
}

void btn_create(Button_t* p)
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
    if (pin == SW) {
        xTaskResumeFromISR(m_btn_task);
    }
}

void btn_work_off(TimerHandle_t xTimer)
{
    __disable_irq();
    vTaskSuspend(m_btn_task);
    __enable_irq();
}

static void btn_task(void* pvParameter)
{
    while (1) {
        Button_Process();
        NRF_LOG_DEBUG("process!!");
        vTaskDelay(30);
    }
}

void btn_init(void)
{
    nrfx_err_t err = nrfx_gpiote_init();
    APP_ERROR_CHECK(err);
    nrfx_gpiote_in_config_t cfg = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(0);
    cfg.pull                    = NRF_GPIO_PIN_PULLUP;
    err                         = nrfx_gpiote_in_init(SW, &cfg, btn_work_on);
    APP_ERROR_CHECK(err);
    nrfx_gpiote_in_event_enable(SW, true);
    // nrf_gpio_cfg_input(SW, NRF_GPIO_PIN_PULLUP);
    btn_create(&SW_BUTTON);
    button_tmr = xTimerCreate(
        "BtnTimer",
        pdMS_TO_TICKS(5000),
        pdFALSE,
        0,
        btn_work_off);
    NRF_LOG_DEBUG("button init!");
    BaseType_t xReturned = xTaskCreate(btn_task,
                                       "BLE",
                                       256,
                                       0,
                                       configMAX_PRIORITIES - 1,
                                       &m_btn_task);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
    vTaskSuspend(m_btn_task);
}

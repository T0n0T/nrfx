/**
 * @file btn_event.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-12
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <rtdevice.h>
#include <stdio.h>
#include <app.h>
#include <button.h>

#define DBG_LVL DBG_INFO
#define DBG_TAG "btn"
#include <rtdbg.h>

Button_t SW_BUTTON;
static struct rt_thread btn_thread;
static char btn_stack[512];

rt_uint8_t read_sw_btn(void)
{
    return rt_pin_read(SW);
}

void btn_double(void)
{
    LOG_D("double click!");
}

void btn_click(void)
{
    LOG_D("single click!");
}

static void btn_entry(void *p)
{
    rt_pin_mode(POWER_KEEP, PIN_MODE_OUTPUT);
    rt_pin_mode(SW, PIN_MODE_INPUT);
    Button_Create(
        "SW",
        &SW_BUTTON,
        read_sw_btn,
        PIN_LOW);
    Button_Attach(&SW_BUTTON, BUTTON_DOWM, btn_click);
    Button_Attach(&SW_BUTTON, BUTTON_DOUBLE, btn_double);
    while (1) {
        Button_Process();
        rt_thread_mdelay(40);
    }
}

int btn_init(void)
{
    if (rt_thread_init(&btn_thread, "btn", btn_entry, 0, btn_stack, sizeof(btn_stack), 13, 15) != RT_EOK) {
        LOG_E("btn thread init fail!\n");
        return -1;
    }
    return rt_thread_startup(&btn_thread);
}
INIT_APP_EXPORT(btn_init);
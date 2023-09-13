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
static char btn_stack[1024];

rt_uint8_t read_sw_btn(void)
{
    return rt_pin_read(SW);
}

void btn_click(void)
{
    LOG_D("single click!");
    rt_pin_write(BEEP, PIN_HIGH);
    rt_thread_mdelay(300);
    rt_pin_write(BEEP, PIN_LOW);
    // if (mission_status) {
    //     publish_handle();
    // }
}

void btn_double(void)
{
    static int flag = 1;
    LOG_D("double click!");
    rt_pin_write(BEEP, PIN_HIGH);
    rt_thread_mdelay(150);
    rt_pin_write(BEEP, PIN_LOW);

    rt_pin_write(BEEP, PIN_HIGH);
    rt_thread_mdelay(150);
    rt_pin_write(BEEP, PIN_LOW);

    if (sm4_flag == 0) {
        printf("sm4 encrypt mode ON !!!!!!!!!\n");
        sm4_flag = 1;
    } else {
        printf("sm4 encrypt mode OFF!!!!!!!!!\n");
        sm4_flag = 0;
    }
}

void btn_long_free(void)
{
    LOG_D("long click!");
    rt_pin_write(BEEP, PIN_HIGH);
    rt_thread_mdelay(50);
    rt_pin_write(BEEP, PIN_LOW);
    rt_thread_mdelay(50);
    rt_pin_write(BEEP, PIN_HIGH);
    rt_thread_mdelay(50);
    rt_pin_write(BEEP, PIN_LOW);
    rt_hw_cpu_reset();
}

static void btn_entry(void *p)
{
    rt_pin_mode(POWER_KEEP, PIN_MODE_OUTPUT);
    rt_pin_mode(LED2, PIN_MODE_OUTPUT);
    rt_pin_mode(LED3, PIN_MODE_OUTPUT);
    rt_pin_mode(BEEP, PIN_MODE_OUTPUT);
    rt_pin_mode(SW, PIN_MODE_INPUT);
    rt_pin_write(LED2, PIN_HIGH);
    rt_pin_write(LED3, PIN_HIGH);
    Button_Create(
        "SW",
        &SW_BUTTON,
        read_sw_btn,
        PIN_LOW);
    Button_Attach(&SW_BUTTON, BUTTON_DOWM, btn_click);
    Button_Attach(&SW_BUTTON, BUTTON_DOUBLE, btn_double);
    Button_Attach(&SW_BUTTON, BUTTON_LONG_FREE, btn_long_free);
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
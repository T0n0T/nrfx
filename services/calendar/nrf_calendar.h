#ifndef __NRF_CALENDAR_H__
#define __NRF_CALENDAR_H__

#include <stdint.h>
#include <stdbool.h>
#include "time.h"


//是否使用UART打印出时间
#define UART_LOG   0

#define UPDATA_SEC    1   //更新秒
#define UPDATA_HM     2   //更新时、分
#define UPDATA_DATE   3   //更新年、月、日


extern uint8_t TimeUpdataFlag;
void nrf_cal_init(void);
char *nrf_cal_get_time_string(void);
void nrf_cal_set_time(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second);
void CAL_updata(void);

#endif

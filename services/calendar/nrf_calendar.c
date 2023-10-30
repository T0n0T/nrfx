/****************************************Copyright (c)************************************************
 **                                      [艾克姆科技]
 **                                        IIKMSIK
 **                            官方店铺：https://acmemcu.taobao.com
 **                            官方论坛：http://www.e930bbs.com
 **
 **--------------------------------------File Info-----------------------------------------------------
 ** Created by:
 ** Created date: 2018-12-1
 ** Version:			 1.1
 ** Last modified Date: 2019-5-23
 ** Descriptions: 时钟日历驱动程序
 **---------------------------------------------------------------------------------------------------*/
#include "nrf_calendar.h"
#include "nrf.h"

// 定义一个tm结构体t
struct tm t;
// 定义时间更新标志，用于更新OLED显示
uint8_t TimeUpdataFlag;

// 设置时间
void nrf_cal_set_time(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second)
{

    t.tm_year = year - 1900;
    t.tm_mon  = month;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
}

// 使用strftime格式化时间
char* nrf_cal_get_time_string(void)
{
    static char cal_string[80];
    strftime(cal_string, 80, "%Y-%m-%d-%H:%M:%S", &t);
    return cal_string;
}
// 判断是不是闰年：能整除4但不能整除100，能整除400
static char not_leap(void)
{
    if (!(t.tm_year % 100)) {
        return (char)(t.tm_year % 400);
    } else {
        return (char)(t.tm_year % 4);
    }
}

void CAL_updata(void)
{
    TimeUpdataFlag = UPDATA_SEC;
    if (++t.tm_sec == 60) // keep track of time, date, month, and year
    {
        TimeUpdataFlag = UPDATA_HM;
        t.tm_sec       = 0;
        if (++t.tm_min == 60) {
            t.tm_min = 0;
            if (++t.tm_hour == 24) {
                TimeUpdataFlag = UPDATA_DATE;
                t.tm_hour      = 0;
                if (++t.tm_mday == 32) {
                    t.tm_mon++;
                    t.tm_mday = 1;
                } else if (t.tm_mday == 31) {
                    // 4,6,9,11是小月（2月单独处理），一月30天
                    if ((t.tm_mon == (4 - 1)) || (t.tm_mon == (6 - 1)) || (t.tm_mon == (9 - 1)) || (t.tm_mon == (11 - 1))) {
                        t.tm_mon++;
                        t.tm_mday = 1;
                    }
                } else if (t.tm_mday == 30) // 2月是闰月，2月有29天
                {
                    if (t.tm_mon == 2) {
                        t.tm_mon++;
                        t.tm_mday = 1;
                    }
                } else if (t.tm_mday == 29) // 2月不是闰月，2月有28天
                {
                    if ((t.tm_mon == 2) && (not_leap())) {
                        t.tm_mon++;
                        t.tm_mday = 1;
                    }
                }
                if (t.tm_mon == 13) {
                    t.tm_mon = 1;
                    t.tm_year++;
                }
            }
        }
    }
}

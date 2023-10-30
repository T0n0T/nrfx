/****************************************Copyright (c)************************************************
 **                                      [����ķ�Ƽ�]
 **                                        IIKMSIK
 **                            �ٷ����̣�https://acmemcu.taobao.com
 **                            �ٷ���̳��http://www.e930bbs.com
 **
 **--------------------------------------File Info-----------------------------------------------------
 ** Created by:
 ** Created date: 2018-12-1
 ** Version:			 1.1
 ** Last modified Date: 2019-5-23
 ** Descriptions: ʱ��������������
 **---------------------------------------------------------------------------------------------------*/
#include "nrf_calendar.h"
#include "nrf.h"

// ����һ��tm�ṹ��t
struct tm t;
// ����ʱ����±�־�����ڸ���OLED��ʾ
uint8_t TimeUpdataFlag;

// ����ʱ��
void nrf_cal_set_time(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second)
{

    t.tm_year = year - 1900;
    t.tm_mon  = month;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
}

// ʹ��strftime��ʽ��ʱ��
char* nrf_cal_get_time_string(void)
{
    static char cal_string[80];
    strftime(cal_string, 80, "%Y-%m-%d-%H:%M:%S", &t);
    return cal_string;
}
// �ж��ǲ������꣺������4����������100��������400
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
                    // 4,6,9,11��С�£�2�µ���������һ��30��
                    if ((t.tm_mon == (4 - 1)) || (t.tm_mon == (6 - 1)) || (t.tm_mon == (9 - 1)) || (t.tm_mon == (11 - 1))) {
                        t.tm_mon++;
                        t.tm_mday = 1;
                    }
                } else if (t.tm_mday == 30) // 2�������£�2����29��
                {
                    if (t.tm_mon == 2) {
                        t.tm_mon++;
                        t.tm_mday = 1;
                    }
                } else if (t.tm_mday == 29) // 2�²������£�2����28��
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

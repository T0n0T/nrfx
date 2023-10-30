#ifndef __NRF_CALENDAR_H__
#define __NRF_CALENDAR_H__

#include <stdint.h>
#include <stdbool.h>
#include "time.h"


//�Ƿ�ʹ��UART��ӡ��ʱ��
#define UART_LOG   0

#define UPDATA_SEC    1   //������
#define UPDATA_HM     2   //����ʱ����
#define UPDATA_DATE   3   //�����ꡢ�¡���


extern uint8_t TimeUpdataFlag;
void nrf_cal_init(void);
char *nrf_cal_get_time_string(void);
void nrf_cal_set_time(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second);
void CAL_updata(void);

#endif

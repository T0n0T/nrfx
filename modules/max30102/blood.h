#ifndef _BLOOD_H
#define _BLOOD_H

#include "algorithm.h"
#include "math.h"

typedef enum {
    HRM_SPO2 = 1,
    PROX,
} HRM_Mode; // 血液状态

typedef struct
{
    int heart;  // 心率数据
    float SpO2; // 血氧数据
} BloodData;

void blood_data_translate(void);
void blood_data_update(void);
void blood_Loop(void);

#endif
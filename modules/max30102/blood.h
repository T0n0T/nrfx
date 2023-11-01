#ifndef _BLOOD_H
#define _BLOOD_H

#include "math.h"
#include "stdint.h"

typedef enum {
    HRM_SPO2 = 1,
    PROX,
} HRM_Mode; // 血液状态

extern int32_t sp02;
extern int32_t heart_rate;
extern uint8_t ecg_status;
void           blood_data_translate(void);
void           blood_data_update(void);
void           blood_Loop(void);

#endif

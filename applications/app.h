/**
 * @file app.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-30
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef __APP_H__
#define __APP_H__

#include <stdio.h>
#include <stdint.h>
#include "ec800m.h"
#include "mqttclient.h"
#include "blood.h"
#include "ble_nus.h"
#include "nrf_fstorage_sd.h"

typedef struct {
    ec800m_mqtt_t mqtt_cfg;
    uint16_t      publish_interval;
    uint8_t       sm4_flag;
    uint8_t       sm4_key[16];
    uint8_t       ble_mac[6];
} config_t;

extern nrf_fstorage_t fstorage;
extern config_t       global_cfg;
extern void           nus_service_init(void);
void                  app_init(void);

/* data build */
char* build_msg_mqtt(char* device_id, gps_info_t info, int energyStatus, int correctlyWear);
char* build_msg_cfg(config_t* config);
int   parse_cfg(char* json, config_t* config);
void  read_cfg_from_flash(void);
void  write_cfg(config_t* cfg);
int   read_cfg(config_t* cfg);

#endif

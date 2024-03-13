/**
 * @file ec800m_common_at_urc.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-03-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "ec800m.h"
#include "ec800m_common.h"

#define NRF_LOG_MODULE_NAME ec800commurc
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

void ec800m_power_on_ready(struct at_client* client, const char* data, size_t size)
{
    NRF_LOG_DEBUG("ec800m power on ready");    
    extern SemaphoreHandle_t      power_sync;
    ec800m_t* dev = (ec800m_t*)client->user_data;
    if (dev->status == EC800M_POWER_OFF) {
        xSemaphoreGive(power_sync);
    }
}

void ec800m_power_off_fin(struct at_client* client, const char* data, size_t size)
{
    NRF_LOG_DEBUG("ec800m power off finish");
    ec800m_t* dev = (ec800m_t*)client->user_data;
    nrf_gpio_pin_write(dev->pwr_pin, 0);
    dev->status = EC800M_POWER_OFF;
}

const struct at_urc comm_urc_table[] = {
    {"RDY", "\r\n", ec800m_power_on_ready},
    {"POWERED DOWN", "\r\n", ec800m_power_off_fin},
};
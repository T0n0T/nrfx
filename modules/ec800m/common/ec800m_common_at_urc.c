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

extern SemaphoreHandle_t comm_sync;
extern int               comm_task_publish(ec800m_comm_task_t task, void* param);

void ec800m_power_on_ready(struct at_client* client, const char* data, size_t size)
{
    NRF_LOG_INFO("ec800m power on ready");
    comm_task_publish(EC800M_TASK_POWERON, NULL);
}

void ec800m_gnss_get_resp(struct at_client* client, const char* data, size_t size)
{
}

const struct at_urc comm_urc_table[] = {
    {"RDY", "\r\n", ec800m_power_on_ready},
};
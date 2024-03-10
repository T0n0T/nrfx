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

void ec800m_gnss_get_resp(struct at_client* client, const char* data, size_t size)
{
    char                   rmc[128] = {0};
    static struct gps_info rmcinfo  = {0};
}
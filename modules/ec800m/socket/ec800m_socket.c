/**
 * @file ec800m_socket.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-12-14
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "ec800m.h"
#include "ec800m_socket.h"

#define NRF_LOG_MODULE_NAME ec800socket
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

EventGroupHandle_t socket_event;

int socket_task_publish(ec800m_socket_task_t task, uint32_t timeout)
{
    ec800m_task_t task_cb = {
        .type    = EC800_SOCKET,
        .task    = EC800M_TASK_SOCKET_OPEN,
        .timeout = timeout,
    };
    return xQueueSend(ec800m.task_queue, &task_cb, 300);
}

int socket_task_wait_response()
{
    EventBits_t rec = xEventGroupWaitBits(socket_event,
                                          EC800M_SOCKET_SUCCESS | EC800M_SOCKET_TIMEOUT | EC800M_SOCKET_ERROR,
                                          pdTRUE,
                                          pdFALSE,
                                          portMAX_DELAY);
    if (rec & EC800M_SOCKET_SUCCESS) {
        return EOK;
    }
    if (rec & EC800M_SOCKET_ERROR) {
        return ERROR;
    }
    if (rec & EC800M_SOCKET_TIMEOUT) {
        return ETIMEOUT;
    }
    return ERROR;
}

void ec800m_socket_init_handle(void)
{
    socket_event = xEventGroupCreate();
    extern const struct at_urc socket_urc_table[];
    at_obj_set_urc_table(ec800m.client, socket_urc_table, 5);
}

void ec800m_socket_task_handle(int task)
{
}

void ec800m_socket_timeout_handle(int task)
{
}

ec800m_task_group_t ec800m_socket_task_group = {
    .id             = EC800_SOCKET,
    .init_handle    = ec800m_socket_init_handle,
    .task_handle    = ec800m_socket_task_handle,
    .timeout_handle = ec800m_socket_timeout_handle,
};
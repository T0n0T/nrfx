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

QueueHandle_t       socket_response;
ec800m_pdp_status_t pdp_status;
ec800m_socket_t     socket[SOCKET_MAX];

int socket_task_publish(ec800m_socket_task_t task, void* param, uint32_t timeout)
{
    ec800m_task_t task_cb = {
        .type    = EC800_MQTT,
        .task    = task,
        .param   = param,
        .timeout = timeout,
    };
    return xQueueSend(ec800m.task_queue, &task_cb, 300);
}

int socket_task_wait_response()
{
    uint32_t result = 0;
    xQueueReceive(socket_response, &result, portMAX_DELAY);
    return result;
}

int ec800m_socket_open(char* domain, char* port, int protocol)
{
    if (pdp_status != EC800M_PDP_ACT) {
        NRF_LOG_ERROR("PDP is not active");
        return ERROR;
    }
    ec800m_socket_t* s;
    for (size_t i = 0; i < SOCKET_MAX; i++) {
        if (socket[i].status == EC800M_SOCKET_IDLE) {
            s         = &socket[i];
            s->socket = i;
            break;
        }
    }
    strcpy(s.domain, domain);
    strcpy(s.port, port);
    s.protocol = protocol;
    socket_task_publish(EC800M_TASK_SOCKET_OPEN, s, 10000);
    return socket_task_wait_response();
}

int ec800m_socket_close(int fd)
{
    socket_task_publish(EC800M_TASK_SOCKET_OPEN, socket[fd], 5000);
}

int ec800m_socket_connect(int fd, const struct sockaddr* addr, socklen_t addrlen)
{
}

int ec800m_socket_read(int fd, void* buf, size_t len, int flags)
{
}

int ec800m_socket_write(int fd, const void* buf, size_t len, int flags)
{
}

void ec800m_socket_init_handle(void)
{
    socket_response = xQueueCreate(1, sizeof(uint32_t));
    extern const struct at_urc socket_urc_table[];
    at_obj_set_urc_table(ec800m.client, socket_urc_table, 4);
    socket_task_publish(EC800M_TASK_SOCKET_INIT);
}

void ec800m_socket_task_handle(int task, void* param)
{
    if (task == EC800M_TASK_SOCKET_INIT) {
        at_cmd_exec(ec800m.client, NULL, AT_CMD_ACT_PDP);
        pdp_status = EC800M_PDP_ACT;
    }
    if (task == EC800M_TASK_SOCKET_OPEN) {
        ec800m_socket_t* s          = (ec800m_socket_t*)param;
        const char*      protocol   = (s->protocol == EC800M_SOCKET_TCP_CLIENT) ? "TCP" : "UDP";
        int              socket_num = s - socket;
        at_cmd_exec(ec800m.client, NULL, AT_CMD_SOCKET_OPEN, socket_num, protocol, s->ip, s->port);
    }
}

void ec800m_socket_timeout_handle(int task)
{
    xQueueOverwrite(socket_response, EC800M_RECV_TIMEOUT);
}

ec800m_task_group_t ec800m_socket_task_group = {
    .id             = EC800_SOCKET,
    .init           = ec800m_socket_init_handle,
    .task_handle    = ec800m_socket_task_handle,
    .timeout_handle = ec800m_socket_timeout_handle,
};
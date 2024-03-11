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
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

QueueHandle_t        socket_response;
ec800m_pdp_status_t  pdp_status;
ec800m_socket_t      socket[SOCKET_MAX];
ec800m_socket_data_t data_recv;

int socket_task_publish(ec800m_socket_task_t task, void* param, uint32_t timeout)
{
    xQueueReset(socket_response);
    NRF_LOG_DEBUG("reset the response!!");
    ec800m_task_t task_cb = {
        .type    = EC800_SOCKET,
        .task    = task,
        .param   = param,
        .timeout = timeout,
    };
    return xQueueSend(ec800m.task_queue, &task_cb, 300);
}

void response(uint32_t result)
{
    NRF_LOG_DEBUG("socket response: %d\r\n", result);
    xQueueOverwrite(socket_response, &result);
}

int socket_task_wait_response()
{
    uint32_t result = 10;
    xQueueReceive(socket_response, &result, portMAX_DELAY);
    NRF_LOG_DEBUG("socket get response: %d", result);
    return result;
}

int ec800m_socket_open(const char* domain, const char* port, int protocol)
{
    uint32_t result = EC800M_SOCKET_SUCCESS;
    if (pdp_status != EC800M_PDP_ACT) {
        socket_task_publish(EC800M_TASK_PDP_DEACT, NULL, 0);
        socket_task_publish(EC800M_TASK_PDP_ACT, NULL, 0);
        socket_task_publish(EC800M_TASK_PDP_CHECK, NULL, 1000);
        result = socket_task_wait_response();
        if (result) {
            NRF_LOG_ERROR("PDP activate fail, err[%d]", result);
            return -ERROR;
        }
    }
    ec800m_socket_t* s = NULL;
    int              i = 0;
    for (; i < SOCKET_MAX; i++) {
        if (socket[i].status == EC800M_SOCKET_IDLE) {
            s = &socket[i];
            break;
        }
    }
    if (s == NULL) {
        NRF_LOG_ERROR("here is no idle socket");
        return -EFULL;
    }

    s->domain   = (char*)domain;
    s->port     = (char*)port;
    s->protocol = protocol;
    socket_task_publish(EC800M_TASK_SOCKET_OPEN, s, 7000);
    result = socket_task_wait_response();
    if (result) {
        NRF_LOG_ERROR("socket open fail, err[%d]", result);
        return -ERROR;
    }
    return i;
}

int ec800m_socket_close(int socket_num)
{
    // socket[socket_num].status = EC800M_SOCKET_IDLE;
    socket_task_publish(EC800M_TASK_SOCKET_CLOSE, &socket[socket_num], 0);
    return EOK;
}

int ec800m_socket_read_with_timeout(int socket_num, void* buf, size_t len, int timeout)
{
    uint32_t              len_s = len;
    ec800m_socket_data_t* data;
    data = &data_recv;
    memset(data, 0, sizeof(ec800m_socket_data_t));
    data->socket_num = socket_num;
    data->buf        = buf;
    data->len        = len;

    if (socket[socket_num].status & EC800M_SOCKET_RECV) {
        xSemaphoreTake(socket[socket_num].recv_sync, 0);
    } else if (xSemaphoreTake(socket[socket_num].recv_sync, timeout) == pdFALSE) {
        return 0;
    }
    socket_task_publish(EC800M_TASK_SOCKET_RECV, data, 0);
    uint32_t result = socket_task_wait_response();
    if (result) {
        NRF_LOG_ERROR("socket recv fail, err[%d]", result);
        return -ERROR;
    }

    if (len_s > data->len) {
        socket[socket_num].status &= ~EC800M_SOCKET_RECV;
    }
    return data->len;
}

int ec800m_socket_write_with_timeout(int socket_num, const void* buf, size_t len, int timeout)
{
    size_t               len_s = len > 1460 ? 1460 : len;
    ec800m_socket_data_t data  = {
         .socket_num = socket_num,
         .buf        = (char*)buf,
         .len        = len_s,
    };
    socket_task_publish(EC800M_TASK_SOCKET_SEND, &data, timeout);
    uint32_t result = socket_task_wait_response();
    if (result) {
        NRF_LOG_ERROR("socket send fail, err[%d]", result);
        return -ERROR;
    }
    return len_s;
}

void ec800m_socket_init_handle(void)
{
    socket_response = xQueueCreate(1, sizeof(uint32_t));
    extern const struct at_urc socket_urc_table[];
    at_obj_set_urc_table(ec800m.client, socket_urc_table, 8);
    for (size_t i = 0; i < SOCKET_MAX; i++) {
        socket[i].recv_sync = xSemaphoreCreateBinary();
    }

    NRF_LOG_INFO("ec800m socket init!")
}

void ec800m_socket_task_handle(int task, void* param)
{
    if (task == EC800M_TASK_PDP_DEACT) {
        at_cmd_exec(ec800m.client, AT_CMD_DEACT_PDP);
    }
    if (task == EC800M_TASK_PDP_ACT) {
        at_cmd_exec(ec800m.client, AT_CMD_ACT_PDP);
    }
    if (task == EC800M_TASK_PDP_CHECK) {
        at_cmd_exec(ec800m.client, AT_CMD_CHECK_PDP);
    }
    if (task == EC800M_TASK_SOCKET_OPEN) {
        ec800m_socket_t* s          = (ec800m_socket_t*)param;
        const char*      protocol   = (s->protocol == EC800M_SOCKET_TCP_CLIENT) ? "TCP" : "UDP";
        int              socket_num = s - socket;
        at_cmd_exec(ec800m.client, AT_CMD_SOCKET_OPEN, socket_num, protocol, s->domain, s->port);
    }
    if (task == EC800M_TASK_SOCKET_CLOSE) {
        ec800m_socket_t* s          = (ec800m_socket_t*)param;
        int              socket_num = s - socket;
        at_cmd_exec(ec800m.client, AT_CMD_SOCKET_CLOSE, socket_num);
    }
    if (task == EC800M_TASK_SOCKET_CHECK) {
        ec800m_socket_t* s          = (ec800m_socket_t*)param;
        int              socket_num = s - socket;
        at_cmd_exec(ec800m.client, AT_CMD_SOCKET_STATUS, socket_num);
    }
    if (task == EC800M_TASK_SOCKET_SEND) {
        ec800m_socket_data_t* data = (ec800m_socket_data_t*)param;
        at_set_end_sign('>');
        at_cmd_exec(ec800m.client, AT_CMD_SOCKET_SEND, data->socket_num, data->len);
        at_client_send(data->buf, data->len);
        at_set_end_sign(0);
    }
    if (task == EC800M_TASK_SOCKET_RECV) {
        ec800m_socket_data_t* data = (ec800m_socket_data_t*)param;
        at_cmd_exec(ec800m.client, AT_CMD_SOCKET_RECV, data->socket_num, data->len);
    }
    if (task == EC800M_TASK_ERR_CHECK) {
        NRF_LOG_WARNING("EC800M: Socket error check");
        at_cmd_exec(ec800m.client, AT_CMD_ERR_CHECK);
    }
}

void ec800m_socket_timeout_handle(int task, void* param)
{
    if (task == EC800M_TASK_SOCKET_CHECK) {
        ec800m_socket_t* s          = (ec800m_socket_t*)param;
        int              socket_num = s - socket;
        socket[socket_num].status   = EC800M_SOCKET_IDLE;
        response(EC800M_SOCKET_IS_RELEASE);
    } else if (task == EC800M_TASK_SOCKET_SEND || task == EC800M_TASK_SOCKET_RECV) {
        ec800m_socket_data_t* data = (ec800m_socket_data_t*)param;
        socket_task_publish(EC800M_TASK_SOCKET_CHECK, &socket[data->socket_num], 1000);
    } else {
        response(EC800M_PROCESS_TIMEOUT);
    }
}

ec800m_task_group_t ec800m_socket_task_group = {
    .id             = EC800_SOCKET,
    .init           = ec800m_socket_init_handle,
    .task_handle    = ec800m_socket_task_handle,
    // .timeout_handle = ec800m_socket_timeout_handle,
};
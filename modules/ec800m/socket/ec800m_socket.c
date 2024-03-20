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

ec800m_pdp_status_t  pdp_status;
ec800m_socket_t      socket[SOCKET_MAX];
ec800m_socket_data_t data_recv;
SemaphoreHandle_t    rx_sync;
SemaphoreHandle_t    tx_sync;
static ec800m_t*     _ec800m;

int socket_task_publish(ec800m_t* dev, ec800m_socket_task_t task, void* param)
{
    ec800m_task_t task_cb = {
        .type  = EC800_SOCKET,
        .task  = task,
        .param = param,
    };
    return xQueueSend(dev->task_queue, &task_cb, EC800M_IPC_MIN_TICK);
}

int ec800m_socket_open(const char* domain, const char* port, int protocol)
{
    int result = EOK;
    if (pdp_status != EC800M_PDP_ACT) {
        socket_task_publish(_ec800m, EC800M_TASK_PDP_DEACT, NULL);
        socket_task_publish(_ec800m, EC800M_TASK_PDP_ACT, NULL);
        socket_task_publish(_ec800m, EC800M_TASK_PDP_CHECK, NULL);
        result = ec800m_wait_sync(_ec800m, portMAX_DELAY);
        if (result < 0) {
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
    socket_task_publish(_ec800m, EC800M_TASK_SOCKET_OPEN, s);
    result = ec800m_wait_sync(_ec800m, 15000);
    if (result < 0) {
        NRF_LOG_ERROR("socket open fail, err[%d]", result);
        return -ERROR;
    }
    return i;
}

int ec800m_socket_close(int socket_num)
{
    socket_task_publish(_ec800m, EC800M_TASK_SOCKET_CLOSE, &socket[socket_num]);
    return ec800m_wait_sync(_ec800m, portMAX_DELAY);;
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
        NRF_LOG_WARNING("ec800m expect value from socket[%d],but no value!", socket_num);
        return 0;
    }
    xSemaphoreTake(rx_sync, 0);
    socket_task_publish(_ec800m, EC800M_TASK_SOCKET_RECV, data);
    if (xSemaphoreTake(rx_sync, timeout) != pdTRUE) {
        NRF_LOG_WARNING("socket recv timeout");
        return -ETIMEOUT;
    } else if (_ec800m->err < 0) {
        NRF_LOG_ERROR("socket recv fail, err[%d]", _ec800m->err);
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
    xSemaphoreTake(tx_sync, 0);
    socket_task_publish(_ec800m, EC800M_TASK_SOCKET_SEND, &data);
    if (xSemaphoreTake(tx_sync, timeout) != pdTRUE) {
        NRF_LOG_WARNING("socket send timeout");
        return -ETIMEOUT;
    } else if (_ec800m->err < 0) {
        NRF_LOG_ERROR("socket send fail, err[%d]", _ec800m->err);
        return -ERROR;
    }

    return len_s;
}

void ec800m_socket_init_handle(ec800m_t* dev)
{
    extern const struct at_urc socket_urc_table[];
    _ec800m = dev;
    at_obj_set_urc_table(dev->client, socket_urc_table, 5);
    for (size_t i = 0; i < SOCKET_MAX; i++) {
        socket[i].recv_sync = xSemaphoreCreateBinary();
    }
    tx_sync = xSemaphoreCreateBinary();
    rx_sync = xSemaphoreCreateBinary();
    NRF_LOG_INFO("ec800m socket init!")
}

void ec800m_socket_task_handle(ec800m_task_t* task_cb, ec800m_t* dev)
{
    xSemaphoreTake(dev->lock, portMAX_DELAY);
    if (task_cb->task == EC800M_TASK_PDP_DEACT) {
        dev->err = at_cmd_exec(dev->client, AT_CMD_DEACT_PDP, NULL);
    }
    if (task_cb->task == EC800M_TASK_PDP_ACT) {
        dev->err = at_cmd_exec(dev->client, AT_CMD_ACT_PDP, NULL);
    }
    if (task_cb->task == EC800M_TASK_PDP_CHECK) {
        char* keyword_line = NULL;
        dev->err           = at_cmd_exec(dev->client, AT_CMD_CHECK_PDP, &keyword_line);
        if (dev->err == EOK) {
            uint32_t pdp_num = 0;
            sscanf(keyword_line, "+QIACT: %d", &pdp_num);
            if (pdp_num) {
                pdp_status = EC800M_PDP_ACT;
            } else {
                pdp_status = EC800M_PDP_DEACT;
                dev->err   = -EINVAL;
            }
        }
        ec800m_post_sync(dev);
    }
    if (task_cb->task == EC800M_TASK_SOCKET_OPEN) {
        ec800m_socket_t* s            = (ec800m_socket_t*)task_cb->param;
        const char*      protocol     = (s->protocol == EC800M_SOCKET_TCP_CLIENT) ? "TCP" : "UDP";
        int              socket_num   = s - socket;
        char*            keyword_line = NULL;
        dev->err                      = at_cmd_exec(dev->client, AT_CMD_SOCKET_OPEN, &keyword_line, socket_num, protocol, s->domain, s->port);

        if (dev->err == EOK) {
            int      socket_num = -1;
            uint32_t err_code   = 0;
            sscanf(keyword_line, "+QIOPEN: %d,%d", &socket_num, &err_code);
            if (socket_num < 0 || socket_num >= SOCKET_MAX) {
                dev->err = -EPERM;
            } else if (err_code == 0) {
                socket[socket_num].status |= EC800M_SOCKET_WORK;
            }
        }
        ec800m_post_sync(dev);
    }
    if (task_cb->task == EC800M_TASK_SOCKET_CLOSE) {
        ec800m_socket_t* s          = (ec800m_socket_t*)task_cb->param;
        int              socket_num = s - socket;
        socket[socket_num].status   = EC800M_SOCKET_IDLE;
        dev->err                    = at_cmd_exec(dev->client, AT_CMD_SOCKET_CLOSE, NULL, socket_num);
        ec800m_post_sync(dev);
    }
    if (task_cb->task == EC800M_TASK_SOCKET_CHECK) {
        ec800m_socket_t* s          = (ec800m_socket_t*)task_cb->param;
        int              socket_num = s - socket;
        dev->err                    = at_cmd_exec(dev->client, AT_CMD_SOCKET_STATUS, NULL, socket_num);
    }
    if (task_cb->task == EC800M_TASK_SOCKET_SEND) {
        ec800m_socket_data_t* data = (ec800m_socket_data_t*)task_cb->param;
        at_set_end_sign('>');
        dev->err = at_cmd_exec(dev->client, AT_CMD_SOCKET_SEND, NULL, data->socket_num, data->len);
        at_client_send(data->buf, data->len);
        at_set_end_sign(0);
    }
    if (task_cb->task == EC800M_TASK_SOCKET_RECV) {
        ec800m_socket_data_t* data = (ec800m_socket_data_t*)task_cb->param;
        dev->err                   = at_cmd_exec(dev->client, AT_CMD_SOCKET_RECV, NULL, data->socket_num, data->len);
    }
    if (task_cb->task == EC800M_TASK_ERR_CHECK) {
        NRF_LOG_WARNING("EC800M: Socket error check");
        dev->err = at_cmd_exec(dev->client, AT_CMD_ERR_CHECK, NULL);
    }
}

static void ec800m_socket_err_handle(ec800m_task_t* task_cb, ec800m_t* dev)
{
    /**
     * @todo err handle
     *
     */
    xSemaphoreGive(dev->lock);
    // if ((task_cb->task == EC800M_TASK_SOCKET_SEND || task_cb->task == EC800M_TASK_SOCKET_RECV) && dev->err < 0) {
    //     ec800m_socket_data_t* data = (ec800m_socket_data_t*)task_cb->param;
    //     socket_task_publish(_ec800m, EC800M_TASK_SOCKET_CHECK, &socket[data->socket_num]);
    //     socket_task_publish(_ec800m, EC800M_TASK_SOCKET_CHECK, &socket[data->socket_num]);
    // }
}

ec800m_task_group_t ec800m_socket_task_group = {
    .id          = EC800_SOCKET,
    .init        = ec800m_socket_init_handle,
    .task_handle = ec800m_socket_task_handle,
    .err_handle  = ec800m_socket_err_handle,
    // .timeout_handle = ec800m_socket_timeout_handle,
};
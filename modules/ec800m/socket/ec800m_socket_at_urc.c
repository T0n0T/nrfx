/**
 * @file ec800m_socket_at_urc.c
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-12-15
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "ec800m.h"
#include "ec800m_socket.h"

#define NRF_LOG_MODULE_NAME ec800socketurc
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

extern QueueHandle_t        socket_response;
extern ec800m_socket_data_t data_recv;

extern int  socket_task_publish(ec800m_socket_task_t task, void* param, uint32_t timeout);
extern void response(uint32_t result);

void socket_open(struct at_client* client, const char* data, size_t size)
{
    int      socket_num = -1;
    uint32_t err_code   = 0;
    xTimerStop(ec800m.timer, 0);
    sscanf(data, "+QIOPEN: %d,%d", &socket_num, &err_code);
    if (socket_num < 0 || socket_num >= SOCKET_MAX) {
        response(EC800M_SOCKET_ERROR);
        return;
    }
    if (err_code == 0) {
        taskENTER_CRITICAL();
        socket[socket_num].status |= EC800M_SOCKET_WORK;
        taskEXIT_CRITICAL();
    }
    response(err_code);
}

void socket_send(struct at_client* client, const char* data, size_t size)
{
    xTimerStop(ec800m.timer, 0);
    if (strstr(data, "SEND OK")) {
        response(EC800M_SOCKET_SUCCESS);
    } else if (strstr(data, "SEND FAIL")) {
        response(EC800M_SOCKET_ERROR);
    }
}

void socket_recv(struct at_client* client, const char* data, size_t size)
{
    xTimerStop(ec800m.timer, 0);
    uint32_t len = 0;
    sscanf(data, "+QIRD: %d", &len);
    if (len) {
        at_client_recv(data_recv.buf, len, 300);
    }
    data_recv.len = len;
    response(EC800M_SOCKET_SUCCESS);
}

void urc_socket_recv(struct at_client* client, const char* data, size_t size)
{
    int socket_num = -1;
    sscanf(data, "+QIURC: \"recv\",%d", &socket_num);
    if (socket_num < 0 || socket_num >= SOCKET_MAX) {
        NRF_LOG_ERROR("One socket has received data,but socket_num wrong");
        return;
    }
    taskENTER_CRITICAL();
    socket[socket_num].status |= EC800M_SOCKET_RECV;
    taskEXIT_CRITICAL();
    xSemaphoreGive(socket[socket_num].recv_sync);
    NRF_LOG_INFO("data come!!!");
}

void urc_socket_close(struct at_client* client, const char* data, size_t size)
{
    int socket_num = -1;
    sscanf(data, "+QIURC: \"closed\",%d", &socket_num);
    if (socket_num < 0 || socket_num >= SOCKET_MAX) {
        NRF_LOG_ERROR("One socket has been close but socket_num wrong");
        return;
    }
    taskENTER_CRITICAL();
    socket[socket_num].status = EC800M_SOCKET_IDLE;
    taskEXIT_CRITICAL();
    socket_task_publish(EC800M_TASK_SOCKET_CLOSE, &socket[socket_num], 0);
}

void urc_socket_pdpdeact(struct at_client* client, const char* data, size_t size)
{
    taskENTER_CRITICAL();
    pdp_status = EC800M_PDP_DEACT;
    taskEXIT_CRITICAL();
}

void urc_socket_qiurc(struct at_client* client, const char* data, size_t size)
{
    switch (*(data + 9)) {
        case 'c':
            urc_socket_close(client, data, size);
            break; //+QIURC: "closed"
        case 'r':
            urc_socket_recv(client, data, size);
            break; //+QIURC: "recv"
        case 'p':
            urc_socket_pdpdeact(client, data, size);
            break; //+QIURC: "pdpdeact"
        default:
            break;
    }
}

void pdp_check_status(struct at_client* client, const char* data, size_t size)
{
    uint32_t pdp_num = 0;
    sscanf(data, "+QIACT: %d", &pdp_num);
    xTimerStop(ec800m.timer, 0);
    if (pdp_num) {
        taskENTER_CRITICAL();
        pdp_status = EC800M_PDP_ACT;
        taskEXIT_CRITICAL();
        response(EC800M_SOCKET_SUCCESS);
    } else {
        taskENTER_CRITICAL();
        pdp_status = EC800M_PDP_DEACT;
        taskEXIT_CRITICAL();
        response(EC800M_PDP_OPEN_FAIL);
    }
}

void socket_check_status(struct at_client* client, const char* data, size_t size)
{
#define SOCKET_INITIAL   0
#define SOCKET_OPENING   1
#define SOCKET_CONNECTED 2
#define SOCKET_LISTENING 3
#define SOCKET_CLOSING   4
    int      socket_num = -1;
    uint32_t state_code = 0;
    sscanf(data, "+QISTATE: %d,%*[^,],%*[^,],%*[^,],%*[^,],%d", &socket_num, &state_code);
    xTimerStop(ec800m.timer, 0);
    if (socket_num < 0 || socket_num >= SOCKET_MAX) {
        response(EC800M_SOCKET_ERROR);
        return;
    }
    if (socket[socket_num].status & EC800M_SOCKET_WORK) {
        NRF_LOG_INFO("socket[%d] status: %d", socket_num, state_code);
        if (state_code == SOCKET_CONNECTED) {
            response(EC800M_SOCKET_SUCCESS);
        }
        if (state_code == SOCKET_CLOSING) {
            taskENTER_CRITICAL();
            socket[socket_num].status = EC800M_SOCKET_IDLE;
            taskEXIT_CRITICAL();
            socket_task_publish(EC800M_TASK_SOCKET_CLOSE, &socket[socket_num], 0);
            response(EC800M_SOCKET_IS_RELEASE);
        }
        return;
    }
    taskENTER_CRITICAL();
    socket[socket_num].status |= EC800M_SOCKET_WORK;
    taskEXIT_CRITICAL();

    response(EC800M_SOCKET_IS_USING);
}

void err_get(struct at_client* client, const char* data, size_t size)
{
    int err_code = 0;
    sscanf(data, "+QIERROR: %d", &err_code);
    NRF_LOG_ERROR("ERR[%d] happened");
    response(err_code);
}

void err_check(struct at_client* client, const char* data, size_t size)
{
    BaseType_t res = socket_task_publish(EC800M_TASK_ERR_CHECK, NULL, 0);
    NRF_LOG_ERROR("Need to check ERR, %d",res);
}

const struct at_urc socket_urc_table[] = {
    {"SEND", "\r\n", socket_send},
    {"+QIRD:", "\r\n", socket_recv},
    {"+QIOPEN:", "\r\n", socket_open},
    {"+QIURC:", "\r\n", urc_socket_qiurc},
    {"+QIACT:", "\r\n", pdp_check_status},
    {"+QISTATE:", "\r\n", socket_check_status},
    {"+QIGETERROR:", "\r\n", err_get},
    {"ERROR", "\r\n", err_check},
};
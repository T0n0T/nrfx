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
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_INFO
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

extern ec800m_socket_data_t data_recv;
extern uint32_t             socket_err_code;

extern int socket_task_publish(ec800m_t* dev, ec800m_socket_task_t task, void* param);

void socket_send(struct at_client* client, const char* data, size_t size)
{
    ec800m_t* dev = (ec800m_t*)client->user_data;
    extern SemaphoreHandle_t tx_sync;
    if (strstr(data, "SEND OK")) {     
    } else if (strstr(data, "SEND FAIL")) {
        dev->err = -EIO;        
    }
    xSemaphoreGive(tx_sync);    
}

void socket_recv(struct at_client* client, const char* data, size_t size)
{
    uint32_t  len = 0;
    ec800m_t* dev = (ec800m_t*)client->user_data;
    extern SemaphoreHandle_t rx_sync;
    sscanf(data, "+QIRD: %d", &len);
    if (len) {
        at_client_recv(data_recv.buf, len, EC800M_IPC_MIN_TICK);
    }
    data_recv.len = len;
    xSemaphoreGive(rx_sync);
}

void urc_socket_recv(struct at_client* client, const char* data, size_t size)
{
    int socket_num = -1;
    sscanf(data, "+QIURC: \"recv\",%d", &socket_num);
    if (socket_num < 0 || socket_num >= SOCKET_MAX) {
        NRF_LOG_ERROR("One socket has received data,but socket_num wrong");
        return;
    }
    socket[socket_num].status |= EC800M_SOCKET_RECV;
    xSemaphoreGive(socket[socket_num].recv_sync);
}

void urc_socket_close(struct at_client* client, const char* data, size_t size)
{
    NRF_LOG_INFO("SOCKET CLOSE!");
    int       socket_num = -1;
    ec800m_t* dev        = (ec800m_t*)client->user_data;
    sscanf(data, "+QIURC: \"closed\",%d", &socket_num);
    if (socket_num < 0 || socket_num >= SOCKET_MAX) {
        NRF_LOG_ERROR("One socket has been close but socket_num wrong");
        return;
    }

    socket[socket_num].status = EC800M_SOCKET_IDLE;
    socket_task_publish(dev, EC800M_TASK_SOCKET_CLOSE, &socket[socket_num]);
}

void urc_socket_pdpdeact(struct at_client* client, const char* data, size_t size)
{
    NRF_LOG_INFO("PDP DEACT!");
    pdp_status = EC800M_PDP_DEACT;
    for (size_t i = 0; i < SOCKET_MAX; i++) {
        socket[i].status = EC800M_SOCKET_IDLE;
    }
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

void socket_check_status(struct at_client* client, const char* data, size_t size)
{
#define SOCKET_INITIAL   0
#define SOCKET_OPENING   1
#define SOCKET_CONNECTED 2
#define SOCKET_LISTENING 3
#define SOCKET_CLOSING   4
    int       socket_num = -1;
    uint32_t  state_code = 0;
    ec800m_t* dev        = (ec800m_t*)client->user_data;
    sscanf(data, "+QISTATE: %d,%*[^,],%*[^,],%*[^,],%*[^,],%d", &socket_num, &state_code);
    if (socket_num < 0 || socket_num >= SOCKET_MAX) {
        NRF_LOG_ERROR("ec800 socket[%d] err!");
        return;
    }
    if (socket[socket_num].status & EC800M_SOCKET_WORK) {
        NRF_LOG_INFO("socket[%d] status: %d", socket_num, state_code);
        if (state_code == SOCKET_CLOSING) {
            socket[socket_num].status = EC800M_SOCKET_IDLE;
            socket_task_publish(dev, EC800M_TASK_SOCKET_CLOSE, &socket[socket_num]);
        }
        return;
    }
    socket[socket_num].status |= EC800M_SOCKET_WORK;
}

void err_get(struct at_client* client, const char* data, size_t size)
{
    uint32_t  err_code = 0;
    ec800m_t* dev      = (ec800m_t*)client->user_data;
    sscanf(data, "+QIGETERROR: %d", &err_code);
    NRF_LOG_ERROR("ERR[%d] happened");
}

void err_check(struct at_client* client, const char* data, size_t size)
{
    // ec800m_t* dev      = (ec800m_t*)client->user_data;
    // socket_task_publish(dev, EC800M_TASK_ERR_CHECK, NULL);
    NVIC_SystemReset();
}

const struct at_urc socket_urc_table[] = {
    {"SEND", "\r\n", socket_send},
    {"+QIRD:", "\r\n", socket_recv},
    {"+QIURC:", "\r\n", urc_socket_qiurc},
    {"+QISTATE:", "\r\n", socket_check_status},
    // {"+QIGETERROR:", "\r\n", err_get},
    {"ERROR", "\r\n", err_check},
};
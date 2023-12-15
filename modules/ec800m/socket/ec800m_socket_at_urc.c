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

extern QueueHandle_t socket_response;
extern int           socket_task_publish(ec800m_socket_task_t task, void* param, uint32_t timeout);

void urc_socket_open(struct at_client* client, const char* data, size_t size)
{
    uint32_t socket_num = 0, err_code = 0;
    sscanf(data, "+QIOPEN: %d,%d", &socket_num, &err_code);
    if (err_code == 0) {
        socket[socket_num].status = EC800M_SOCKET_OPEN;
    }
    xQueueOverwrite(socket_response, err_code);
}

void urc_socket_connect(struct at_client* client, const char* data, size_t size)
{
}

void urc_socket_send(struct at_client* client, const char* data, size_t size)
{
}

void urc_socket_recv(struct at_client* client, const char* data, size_t size)
{
}

void urc_socket_close(struct at_client* client, const char* data, size_t size)
{
}

void urc_socket_pdpdeact(struct at_client* client, const char* data, size_t size)
{
}

static void urc_socket_qiurc(struct at_client* client, const char* data, size_t size)
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

const struct at_urc socket_urc_table[] = {
    {"SEND OK", "\r\n", urc_socket_send},
    {"SEND FAIL", "\r\n", urc_socket_send},
    {"+QIOPEN:", "\r\n", urc_socket_open},
    {"+QIURC:", "\r\n", urc_socket_qiurc},
};
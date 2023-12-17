/**
 * @file ec800m_socket.h
 * @author T0n0T (T0n0T@823478402@qq.com)
 * @brief
 * @version 0.1
 * @date 2023-12-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __EC800M_SOCKET_H__
#define __EC800M_SOCKET_H__

#include "stdio.h"
#include "stdint.h"

#include "FreeRTOS.h"
#include "semphr.h"

#define SOCKET_MAX                 5

#define EC800M_SOCKET_SUCCESS      0
#define EC800M_SOCKET_ERROR        1
#define EC800M_SOCKET_IS_RELEASE   2
#define EC800M_SOCKET_IS_USING     3
#define EC800M_PROCESS_TIMEOUT     4
#define EC800M_SOCKET_UNKOWN_WRONG 550
#define EC800M_OPERAT_BLOCK        551
#define EC800M_INVAILD_PARAM       552
#define EC800M_SOCKET_MEMORY_FULL  553
#define EC800M_SOCKET_CREATE_FAIL  554
#define EC800M_SOCKET_NOT_SUPPORT  555
#define EC800M_SOCKET_BIND_FAIL    556
#define EC800M_SOCKET_LIST_FAIL    557
#define EC800M_SOCKET_WRITE_FAIL   558
#define EC800M_SOCKET_READ_FAIL    559
#define EC800M_SOCKET_ACCEPT_FAIL  560
#define EC800M_PDP_OPEN_FAIL       561
#define EC800M_PDP_CLOSE_FAIL      562
#define EC800M_SOCKET_IS_USING     563
#define EC800M_DNS_BUSY            564
#define EC800M_DNS_FAIL            565
#define EC800M_SOCKET_CONN_FAIL    566
#define EC800M_SOCKET_IS_CLOSED    567
#define EC800M_SOCKET_BUSY         568
#define EC800M_SOCKET_TIMEOUT      569
#define EC800M_PDP_BROKEN          570
#define EC800M_SOCKET_SEND_CANCLE  571
#define EC800M_OPERAT_FORBID       572
#define EC800M_APN_NOT_CONF        573
#define EC800M_SOCKET_PORT_BUSY    574

#define EC800M_SOCKET_TCP_CLIENT   1
#define EC800M_SOCKET_UDP_CLIENT   2
#define EC800M_SOCKET_TCP_SERVER   3
#define EC800M_SOCKET_UDP_SERVER   4

#define EC800M_SOCKET_IDLE         0
#define EC800M_SOCKET_WORK         (1 << 0)
#define EC800M_SOCKET_RECV         (1 << 1)

typedef enum {
    EC800M_PDP_DEACT,
    EC800M_PDP_ACT,
} ec800m_pdp_status_t;

typedef enum {
    EC800M_TASK_PDP_DEACT = 1,
    EC800M_TASK_PDP_ACT,
    EC800M_TASK_PDP_CHECK,
    EC800M_TASK_SOCKET_OPEN,
    EC800M_TASK_SOCKET_CLOSE,
    EC800M_TASK_SOCKET_CHECK,
    EC800M_TASK_SOCKET_SEND,
    EC800M_TASK_SOCKET_RECV,
    EC800M_TASK_ERR_CHECK,
} ec800m_socket_task_t;

typedef struct {
    char*             domain;
    char*             port;
    uint32_t          protocol;
    uint32_t          status;
    SemaphoreHandle_t recv_sync;
} ec800m_socket_t;

typedef struct {
    int    socket_num;
    char*  buf;
    size_t len;
} ec800m_socket_data_t;

extern ec800m_pdp_status_t pdp_status;
extern ec800m_socket_t     socket[];

int ec800m_socket_open(const char* domain, const char* port, int protocol);
int ec800m_socket_close(int socket_num);
int ec800m_socket_read_with_timeout(int socket_num, void* buf, size_t len, int timeout);
int ec800m_socket_write_with_timeout(int socket_num, const void* buf, size_t len, int timeout);

#endif
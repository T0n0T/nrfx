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

#define SOCKET_MAX                 5

#define EC800M_SOCKET_SUCCESS      0
#define EC800M_RECV_TIMEOUT        1
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

#define EC800M_SOCKET_ERROR        (1 << 2)

#define EC800M_SOCKET_TCP_CLIENT   1
#define EC800M_SOCKET_UDP_CLIENT   2
#define EC800M_SOCKET_TCP_SERVER   3
#define EC800M_SOCKET_UDP_SERVER   4

typedef enum {
    EC800M_PDP_DEACT,
    EC800M_PDP_ACT,
} ec800m_pdp_status_t;

typedef enum {
    EC800M_SOCKET_IDLE,
    EC800M_SOCKET_OPEN,
    EC800M_SOCKET_CONN,
    EC800M_SOCKET_DISC,
} ec800m_socket_status_t;

typedef enum {
    EC800M_TASK_SOCKET_CHECK = 1,
    EC800M_TASK_SOCKET_INIT,
    EC800M_TASK_SOCKET_OPEN,
    EC800M_TASK_SOCKET_CONNECT,
    EC800M_TASK_SOCKET_SEND,
    EC800M_TASK_SOCKET_RECV,
    EC800M_TASK_SOCKET_CLOSE,
} ec800m_socket_task_t;

typedef struct {
    char*                  domain;
    char*                  port;
    uint32_t               protocol;
    ec800m_socket_status_t status;
} ec800m_socket_t;

extern ec800m_pdp_status_t pdp_status;
extern ec800m_socket_t     socket;

#endif
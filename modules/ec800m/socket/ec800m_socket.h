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

#define EC800M_SOCKET_SUCCESS (1 << 0)
#define EC800M_SOCKET_TIMEOUT (1 << 1)
#define EC800M_SOCKET_ERROR   (1 << 2)
typedef enum {
    EC800M_TASK_SOCKET_CHECK = 1,
    EC800M_TASK_SOCKET_INIT,
    EC800M_TASK_SOCKET_OPEN,
    EC800M_TASK_SOCKET_CONNECT,
    EC800M_TASK_SOCKET_SEND,
    EC800M_TASK_SOCKET_RECV,
    EC800M_TASK_SOCKET_CLOSE,
} ec800m_socket_task_t;

#endif
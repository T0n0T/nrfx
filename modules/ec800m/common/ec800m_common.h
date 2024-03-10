/**
 * @file ec800m_common.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2024-03-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef EC800M_COMMON_H
#define EC800M_COMMON_H

typedef enum {
    EC800M_TASK_POWER_ON = 1,
    EC800M_TASK_POWER_OFF,
    EC800M_TASK_GNSS_CHECK,
    EC800M_TASK_GNSS_OPEN,
    EC800M_TASK_GNSS_GET,
    EC800M_TASK_SIGNAL_CHECK,
    EC800M_TASK_SOCKET_SEND,
    EC800M_TASK_SOCKET_RECV,
    EC800M_TASK_ERR_CHECK,
} ec800m_comm_task_t;

#endif
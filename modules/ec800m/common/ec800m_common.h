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
	EC800M_TASK_POWERON = 1,
    EC800M_TASK_POWEROFF,
    EC800M_TASK_POWERLOW,
    EC800M_TASK_SIGNAL_CHECK,
	EC800M_TASK_GNSS_CHECK,
	EC800M_TASK_GNSS_OPEN,
	EC800M_TASK_GNSS_GET,
} ec800m_comm_task_t;

#endif
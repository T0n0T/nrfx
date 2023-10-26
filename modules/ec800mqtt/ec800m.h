/**
 * @file ec800m.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-26
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef _EC800M_H_
#define _EC800M_H_

#include "at.h"

struct ec800m_t {
    at_client_t client;
    uint32_t    pwr_pin;
    uint32_t    wakeup_pin;
    bool        is_init;
};

struct at_cmd_t {
    char*   desc;
    char*   cmd_expr;
    int32_t timeout;
}

#define AT_CMD_TABLE                             \
    {                                            \
        { "test", "AT", 3 },                     \
            { "echo off", "ATE0", 300 },         \
            { "low power", "AT+QSCLK=%d", 300 }, \
            { "ping", "AT+QSCLK=%d", 300 },      \
    }

#endif
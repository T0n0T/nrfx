/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-03-30     chenyong     first version
 * 2018-08-17     chenyong     multiple client support
 */

#ifndef __AT_H__
#define __AT_H__

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "nrfx_uart.h"

#define AT_PRINT_RAW_CMD

extern bool transfer_status;

#define EOK      0  /**< There is no error */
#define ERROR    1  /**< A generic error happens */
#define ETIMEOUT 2  /**< Timed out */
#define EFULL    3  /**< The resource is full */
#define EEMPTY   4  /**< The resource is empty */
#define ENOMEM   5  /**< No memory */
#define ENOSYS   6  /**< No system */
#define EBUSY    7  /**< Busy */
#define EIO      8  /**< IO error */
#define EINTR    9  /**< Interrupted system call */
#define EINVAL   10 /**< Invalid argument */
#define ETRAP    11 /**< Trap event */
#define ENOENT   12 /**< No entry */
#define ENOSPC   13 /**< No space left */
#define EPERM    14 /**< Operation not permitted */

#ifdef __cplusplus
extern "C" {
#endif

#define NAME_MAX        8
#define AT_SW_VERSION   "1.3.1"

#define AT_CMD_NAME_LEN 16
#define AT_END_MARK_LEN 4

#ifndef AT_CMD_MAX_LEN
#define AT_CMD_MAX_LEN 128
#endif

/* the server AT commands new line sign */
#if defined(AT_CMD_END_MARK_CRLF)
#define AT_CMD_END_MARK "\r\n"
#elif defined(AT_CMD_END_MARK_CR)
#define AT_CMD_END_MARK "\r"
#elif defined(AT_CMD_END_MARK_LF)
#define AT_CMD_END_MARK "\n"
#endif

#ifndef AT_SERVER_RECV_BUFF_LEN
#define AT_SERVER_RECV_BUFF_LEN 256
#endif

/* the maximum number of supported AT clients */
#ifndef AT_CLIENT_NUM_MAX
#define AT_CLIENT_NUM_MAX 1
#endif

enum at_status {
    AT_STATUS_UNINITIALIZED = 0,
    AT_STATUS_INITIALIZED,
    AT_STATUS_CLI,
};
typedef enum at_status at_status_t;

#ifdef AT_USING_CLIENT
enum at_resp_status {
    AT_RESP_OK        = 0,  /* AT response end is OK */
    AT_RESP_ERROR     = -1, /* AT response end is ERROR */
    AT_RESP_TIMEOUT   = -2, /* AT response is timeout */
    AT_RESP_BUFF_FULL = -3, /* AT response buffer is full */
};
typedef enum at_resp_status at_resp_status_t;

struct at_response {
    /* response buffer */
    char* buf;
    /* the maximum response buffer size, it set by `at_create_resp()` function */
    size_t buf_size;
    /* the length of current response buffer */
    size_t buf_len;
    /* the number of setting response lines, it set by `at_create_resp()` function
     * == 0: the response data will auto return when received 'OK' or 'ERROR'
     * != 0: the response data will return when received setting lines number data */
    size_t line_num;
    /* the count of received response lines */
    size_t line_counts;
    /* the maximum response time */
    int32_t timeout;
};

typedef struct at_response* at_response_t;

struct at_client;

/* URC(Unsolicited Result Code) object, such as: 'RING', 'READY' request by AT server */
struct at_urc {
    const char* cmd_prefix;
    const char* cmd_suffix;
    void (*func)(struct at_client* client, const char* data, size_t size);
};
typedef struct at_urc* at_urc_t;

struct at_urc_table {
    size_t               urc_size;
    const struct at_urc* urc;
};
typedef struct at_urc* at_urc_table_t;

struct at_client {
    nrfx_uart_t*         device;
    nrfx_uart_config_t*  cfg;
    StreamBufferHandle_t rx_buf;

    at_status_t status;
    char        end_sign;

    /* the current received one line data buffer */
    char* recv_line_buf;
    /* The length of the currently received one line data */
    size_t recv_line_len;
    /* The maximum supported receive data length */
    size_t recv_bufsz;
    // SemaphoreHandle_t rx_notice;
    SemaphoreHandle_t lock;
    SemaphoreHandle_t rx_notice;

    at_response_t     resp;
    SemaphoreHandle_t resp_notice;
    at_resp_status_t  resp_status;

    struct at_urc_table* urc_table;
    size_t               urc_table_size;

    TaskHandle_t parser;
};
typedef struct at_client* at_client_t;
#endif /* AT_USING_CLIENT */

#ifdef AT_USING_CLIENT

/* AT client initialize and start*/
int at_client_init(const char* dev_name, size_t recv_bufsz);

/* ========================== multiple AT client function ============================ */

/* get AT client object */
at_client_t at_client_get(const char* dev_name);
at_client_t at_client_get_first(void);

/* AT client wait for connection to external devices. */
int at_client_obj_wait_connect(at_client_t client, uint32_t timeout);

/* AT client send or receive data */
size_t at_client_obj_send(at_client_t client, const char* buf, size_t size);
size_t at_client_obj_recv(at_client_t client, char* buf, size_t size, int32_t timeout);

/* set AT client a line end sign */
void at_obj_set_end_sign(at_client_t client, char ch);

/* Set URC(Unsolicited Result Code) table */
int at_obj_set_urc_table(at_client_t client, const struct at_urc* table, size_t size);

/* AT client send commands to AT server and waiter response */
int at_obj_exec_cmd(at_client_t client, at_response_t resp, const char* cmd_expr, ...);

/* AT response object create and delete */
at_response_t at_create_resp(size_t buf_size, size_t line_num, int32_t timeout);
void          at_delete_resp(at_response_t resp);
at_response_t at_resp_set_info(at_response_t resp, size_t buf_size, size_t line_num, int32_t timeout);

/* AT response line buffer get and parse response buffer arguments */
const char* at_resp_get_line(at_response_t resp, size_t resp_line);
const char* at_resp_get_line_by_kw(at_response_t resp, const char* keyword);
int         at_resp_parse_line_args(at_response_t resp, size_t resp_line, const char* resp_expr, ...);
int         at_resp_parse_line_args_by_kw(at_response_t resp, const char* keyword, const char* resp_expr, ...);

/* ========================== single AT client function ============================ */

/**
 * NOTE: These functions can be used directly when there is only one AT client.
 * If there are multiple AT Client in the program, these functions can operate on the first initialized AT client.
 */

#define at_exec_cmd(resp, ...)                at_obj_exec_cmd(at_client_get_first(), resp, __VA_ARGS__)
#define at_client_wait_connect(timeout)       at_client_obj_wait_connect(at_client_get_first(), timeout)
#define at_client_send(buf, size)             at_client_obj_send(at_client_get_first(), buf, size)
#define at_client_recv(buf, size, timeout)    at_client_obj_recv(at_client_get_first(), buf, size, timeout)
#define at_set_end_sign(ch)                   at_obj_set_end_sign(at_client_get_first(), ch)
#define at_set_urc_table(urc_table, table_sz) at_obj_set_urc_table(at_client_get_first(), urc_table, table_sz)

#endif /* AT_USING_CLIENT */

/* ========================== User port function ============================ */

#ifdef __cplusplus
}
#endif

#endif /* __AT_H__ */

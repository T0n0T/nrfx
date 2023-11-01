/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-03-30     chenyong     first version
 * 2018-04-12     chenyong     add client implement
 * 2018-08-17     chenyong     multiple client support
 * 2021-03-17     Meco Man     fix a buf of leaking memory
 * 2021-07-14     Sszl         fix a buf of leaking memory
 */

#include <at.h>

#define NRF_LOG_MODULE_NAME atclient
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

nrfx_uart_t        p_instance = NRFX_UART_INSTANCE(0);
nrfx_uart_config_t config     = NRFX_UART_DEFAULT_CONFIG;
static char        rx_ch;

#ifdef AT_USING_CLIENT

#define AT_RESP_END_OK    "OK"
#define AT_RESP_END_ERROR "ERROR"
#define AT_RESP_END_FAIL  "FAIL"
#define AT_END_CR_LF      "\r\n"

static struct at_client at_client_table[AT_CLIENT_NUM_MAX] = {0};

extern size_t      at_utils_send(void*       dev,
                                 uint32_t    pos,
                                 const void* buffer,
                                 size_t      size);
extern size_t      at_vprintfln(void* device, const char* format, va_list args);
extern void        at_print_raw_cmd(const char* type, const char* cmd, size_t size);
extern const char* at_get_last_cmd(size_t* cmd_size);

/**
 * Create response object.
 *
 * @param buf_size the maximum response buffer size
 * @param line_num the number of setting response lines
 *         = 0: the response data will auto return when received 'OK' or 'ERROR'
 *        != 0: the response data will return when received setting lines number data
 * @param timeout the maximum response time
 *
 * @return != NULL: response object
 *          = NULL: no memory
 */
at_response_t at_create_resp(size_t buf_size, size_t line_num, int32_t timeout)
{
    at_response_t resp = NULL;

    resp = (at_response_t)calloc(1, sizeof(struct at_response));
    if (resp == NULL) {
        NRF_LOG_ERROR("AT create response object failed! No memory for response object!");
        return NULL;
    }

    resp->buf = (char*)calloc(1, buf_size);
    if (resp->buf == NULL) {
        NRF_LOG_ERROR("AT create response object failed! No memory for response buffer!");
        free(resp);
        return NULL;
    }

    resp->buf_size    = buf_size;
    resp->line_num    = line_num;
    resp->line_counts = 0;
    resp->timeout     = timeout;

    return resp;
}

/**
 * Delete and free response object.
 *
 * @param resp response object
 */
void at_delete_resp(at_response_t resp)
{
    if (resp && resp->buf) {
        free(resp->buf);
    }

    if (resp) {
        free(resp);
        resp = NULL;
    }
}

/**
 * Set response object information
 *
 * @param resp response object
 * @param buf_size the maximum response buffer size
 * @param line_num the number of setting response lines
 *         = 0: the response data will auto return when received 'OK' or 'ERROR'
 *        != 0: the response data will return when received setting lines number data
 * @param timeout the maximum response time
 *
 * @return  != NULL: response object
 *           = NULL: no memory
 */
at_response_t at_resp_set_info(at_response_t resp, size_t buf_size, size_t line_num, int32_t timeout)
{
    char* p_temp;
    ASSERT(resp);

    if (resp->buf_size != buf_size) {
        resp->buf_size = buf_size;

        p_temp = (char*)realloc(resp->buf, buf_size);
        if (p_temp == NULL) {
            NRF_LOG_DEBUG("No memory for realloc response buffer size(%d).", buf_size);
            return NULL;
        } else {
            resp->buf = p_temp;
        }
    }

    resp->line_num = line_num;
    resp->timeout  = timeout;

    return resp;
}

/**
 * Get one line AT response buffer by line number.
 *
 * @param resp response object
 * @param resp_line line number, start from '1'
 *
 * @return != NULL: response line buffer
 *          = NULL: input response line error
 */
const char* at_resp_get_line(at_response_t resp, size_t resp_line)
{
    char*  resp_buf      = resp->buf;
    char*  resp_line_buf = NULL;
    size_t line_num      = 1;

    ASSERT(resp);

    if (resp_line > resp->line_counts || resp_line <= 0) {
        NRF_LOG_ERROR("AT response get line failed! Input response line(%d) error!", resp_line);
        return NULL;
    }

    for (line_num = 1; line_num <= resp->line_counts; line_num++) {
        if (resp_line == line_num) {
            resp_line_buf = resp_buf;

            return resp_line_buf;
        }

        resp_buf += strlen(resp_buf) + 1;
    }

    return NULL;
}

/**
 * Get one line AT response buffer by keyword
 *
 * @param resp response object
 * @param keyword query keyword
 *
 * @return != NULL: response line buffer
 *          = NULL: no matching data
 */
const char* at_resp_get_line_by_kw(at_response_t resp, const char* keyword)
{
    char*  resp_buf      = resp->buf;
    char*  resp_line_buf = NULL;
    size_t line_num      = 1;

    ASSERT(resp);
    ASSERT(keyword);

    for (line_num = 1; line_num <= resp->line_counts; line_num++) {
        if (strstr(resp_buf, keyword)) {
            resp_line_buf = resp_buf;

            return resp_line_buf;
        }

        resp_buf += strlen(resp_buf) + 1;
    }

    return NULL;
}

/**
 * Get and parse AT response buffer arguments by line number.
 *
 * @param resp response object
 * @param resp_line line number, start from '1'
 * @param resp_expr response buffer expression
 *
 * @return -1 : input response line number error or get line buffer error
 *          0 : parsed without match
 *         >0 : the number of arguments successfully parsed
 */
int at_resp_parse_line_args(at_response_t resp, size_t resp_line, const char* resp_expr, ...)
{
    va_list     args;
    int         resp_args_num = 0;
    const char* resp_line_buf = NULL;

    ASSERT(resp);
    ASSERT(resp_expr);

    if ((resp_line_buf = at_resp_get_line(resp, resp_line)) == NULL) {
        return -1;
    }

    va_start(args, resp_expr);

    resp_args_num = vsscanf(resp_line_buf, resp_expr, args);

    va_end(args);

    return resp_args_num;
}

/**
 * Get and parse AT response buffer arguments by keyword.
 *
 * @param resp response object
 * @param keyword query keyword
 * @param resp_expr response buffer expression
 *
 * @return -1 : input keyword error or get line buffer error
 *          0 : parsed without match
 *         >0 : the number of arguments successfully parsed
 */
int at_resp_parse_line_args_by_kw(at_response_t resp, const char* keyword, const char* resp_expr, ...)
{
    va_list     args;
    int         resp_args_num = 0;
    const char* resp_line_buf = NULL;

    ASSERT(resp);
    ASSERT(resp_expr);

    if ((resp_line_buf = at_resp_get_line_by_kw(resp, keyword)) == NULL) {
        return -1;
    }

    va_start(args, resp_expr);

    resp_args_num = vsscanf(resp_line_buf, resp_expr, args);

    va_end(args);

    return resp_args_num;
}

/**
 * Send commands to AT server and wait response.
 *
 * @param client current AT client object
 * @param resp AT response object, using NULL when you don't care response
 * @param cmd_expr AT commands expression
 *
 * @return 0 : success
 *        -1 : response status error
 *        -2 : wait timeout
 *        -7 : enter AT CLI mode
 */
int at_obj_exec_cmd(at_client_t client, at_response_t resp, const char* cmd_expr, ...)
{
    va_list     args;
    size_t      cmd_size = 0;
    uint32_t    result   = EOK;
    const char* cmd      = NULL;

    ASSERT(cmd_expr);

    if (client == NULL) {
        NRF_LOG_ERROR("input AT Client object is NULL, please create or get AT Client object!");
        return -ERROR;
    }

    /* check AT CLI mode */
    if (client->status == AT_STATUS_CLI && resp) {
        return -EBUSY;
    }

    xSemaphoreTake(client->lock, portMAX_DELAY);

    client->resp_status = AT_RESP_OK;

    if (resp != NULL) {
        resp->buf_len     = 0;
        resp->line_counts = 0;
    }

    client->resp = resp;

    /* clear the receive buffer */
    xQueueReset(client->rx_queue);

    /* clear the current received one line data buffer, Ignore dirty data before transmission */
    memset(client->recv_line_buf, 0x00, client->recv_line_len);
    client->recv_line_len = 0;

    va_start(args, cmd_expr);
    at_vprintfln(client->device, cmd_expr, args);
    va_end(args);

    if (resp != NULL) {
        if (xSemaphoreTake(client->resp_notice, pdMS_TO_TICKS(resp->timeout)) != pdTRUE) {
            cmd = at_get_last_cmd(&cmd_size);
            NRF_LOG_WARNING("execute command (%s) timeout (%d ticks)!", cmd, resp->timeout);
            taskDISABLE_INTERRUPTS();
            vQueueDelete(client->rx_queue);
            client->rx_queue = xQueueCreate(128, sizeof(char*));
            taskENABLE_INTERRUPTS();
            client->resp_status = AT_RESP_TIMEOUT;
            result              = -ETIMEOUT;
            goto __exit;
        }
        if (client->resp_status != AT_RESP_OK) {
            cmd = at_get_last_cmd(&cmd_size);
            NRF_LOG_ERROR("execute command (%.*s) failed!", cmd_size, cmd);
            result = -ERROR;
            goto __exit;
        }
    }

__exit:
    client->resp = NULL;

    xSemaphoreGive(client->lock);

    return result;
}

/**
 * Waiting for connection to external devices.
 *
 * @param client current AT client object
 * @param timeout millisecond for timeout
 *
 * @return 0 : success
 *        -2 : timeout
 *        -5 : no memory
 */
int at_client_obj_wait_connect(at_client_t client, uint32_t timeout)
{
    uint32_t      result     = EOK;
    at_response_t resp       = NULL;
    uint32_t      start_time = 0;

    if (client == NULL) {
        NRF_LOG_ERROR("input AT client object is NULL, please create or get AT Client object!");
        return -ERROR;
    }

    resp = at_create_resp(64, 0, 300);
    if (resp == NULL) {
        NRF_LOG_ERROR("no memory for AT client(%s) response object.");
        return -ENOMEM;
    }

    xSemaphoreTake(client->lock, portMAX_DELAY);
    client->resp = resp;

    start_time = xTaskGetTickCount();

    while (1) {
        /* Check whether it is timeout */
        if (xTaskGetTickCount() - start_time > pdMS_TO_TICKS(timeout)) {
            NRF_LOG_ERROR("wait AT client(%s) connect timeout(%d tick).", timeout);
            result = -ETIMEOUT;
            break;
        }

        /* Check whether it is already connected */
        resp->buf_len     = 0;
        resp->line_counts = 0;
        at_utils_send(client->device, 0, "AT\r\n", 4);

        if (xSemaphoreTake(client->resp_notice, pdMS_TO_TICKS(resp->timeout)) != pdTRUE)
            continue;
        else
            break;
    }

    at_delete_resp(resp);

    client->resp = NULL;

    xSemaphoreGive(client->lock);

    return result;
}

/**
 * Send data to AT server, send data don't have end sign(eg: \r\n).
 *
 * @param client current AT client object
 * @param buf   send data buffer
 * @param size  send fixed data size
 *
 * @return >0: send data size
 *         =0: send failed
 */
size_t at_client_obj_send(at_client_t client, const char* buf, size_t size)
{
    size_t len;

    ASSERT(buf);

    if (client == NULL) {
        NRF_LOG_ERROR("input AT Client object is NULL, please create or get AT Client object!");
        return 0;
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("sendline", buf, size);
#endif

    xSemaphoreTake(client->lock, portMAX_DELAY);
    len = at_utils_send(client->device, 0, buf, size);
    xSemaphoreGive(client->lock);

    return len;
}

static uint32_t at_client_getchar(at_client_t client, char* ch, TickType_t timeout)
{
    xQueueReceive(client->rx_queue, ch, timeout);
    return EOK;
}

/**
 * AT client receive fixed-length data.
 *
 * @param client current AT client object
 * @param buf   receive data buffer
 * @param size  receive fixed data size
 * @param timeout  receive data timeout (ms)
 *
 * @note this function can only be used in execution function of URC data
 *
 * @return >0: receive data size
 *         =0: receive failed
 */
size_t at_client_obj_recv(at_client_t client, char* buf, size_t size, int32_t timeout)
{
    char ch;
    int  read_idx = 0;

    ASSERT(buf);

    if (client == NULL) {
        NRF_LOG_ERROR("input AT Client object is NULL, please create or get AT Client object!");
        return 0;
    }

    while (1) {
        if (read_idx < size) {
            uint32_t result = at_client_getchar(client, &ch, pdMS_TO_TICKS(timeout));
            if (result != EOK) {
                NRF_LOG_ERROR("AT Client receive failed, uart device get data error(%d)", result);
                return 0;
            }
            buf[read_idx++] = ch;
        } else {
            break;
        }
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("urc_recv", buf, read_idx);
#endif

    return read_idx;
}

/**
 *  AT client set end sign.
 *
 * @param client current AT client object
 * @param ch the end sign, can not be used when it is '\0'
 */
void at_obj_set_end_sign(at_client_t client, char ch)
{
    if (client == NULL) {
        NRF_LOG_ERROR("input AT Client object is NULL, please create or get AT Client object!");
        return;
    }

    client->end_sign = ch;
}

/**
 * set URC(Unsolicited Result Code) table
 *
 * @param client current AT client object
 * @param table URC table
 * @param size table size
 */
int at_obj_set_urc_table(at_client_t client, const struct at_urc* urc_table, size_t table_sz)
{
    size_t idx;

    if (client == NULL) {
        NRF_LOG_ERROR("input AT Client object is NULL, please create or get AT Client object!");
        return -ERROR;
    }

    for (idx = 0; idx < table_sz; idx++) {
        ASSERT(urc_table[idx].cmd_prefix);
        ASSERT(urc_table[idx].cmd_suffix);
    }

    if (client->urc_table_size == 0) {
        client->urc_table = (struct at_urc_table*)calloc(1, sizeof(struct at_urc_table));
        if (client->urc_table == NULL) {
            return -ENOMEM;
        }

        client->urc_table[0].urc      = urc_table;
        client->urc_table[0].urc_size = table_sz;
        client->urc_table_size++;
    } else {
        struct at_urc_table* new_urc_table = NULL;

        /* realloc urc table space */
        new_urc_table = (struct at_urc_table*)realloc(client->urc_table, client->urc_table_size * sizeof(struct at_urc_table) + sizeof(struct at_urc_table));
        if (new_urc_table == NULL) {
            return -ENOMEM;
        }
        client->urc_table                                  = new_urc_table;
        client->urc_table[client->urc_table_size].urc      = urc_table;
        client->urc_table[client->urc_table_size].urc_size = table_sz;
        client->urc_table_size++;
    }

    return EOK;
}

/**
 * get AT client object by AT device name.
 *
 * @dev_name AT client device name
 *
 * @return AT client object
 */
at_client_t at_client_get(const char* dev_name)
{
    return &at_client_table[0];
}

/**
 * get first AT client object in the table.
 *
 * @return AT client object
 */
at_client_t at_client_get_first(void)
{
    if (at_client_table[0].device == NULL) {
        return NULL;
    }

    return &at_client_table[0];
}

static const struct at_urc* get_urc_obj(at_client_t client)
{
    size_t               i, j, prefix_len, suffix_len;
    size_t               bufsz;
    char*                buffer    = NULL;
    const struct at_urc* urc       = NULL;
    struct at_urc_table* urc_table = NULL;

    if (client->urc_table == NULL) {
        return NULL;
    }

    buffer = client->recv_line_buf;
    bufsz  = client->recv_line_len;

    for (i = 0; i < client->urc_table_size; i++) {
        for (j = 0; j < client->urc_table[i].urc_size; j++) {
            urc_table = client->urc_table + i;
            urc       = urc_table->urc + j;

            prefix_len = strlen(urc->cmd_prefix);
            suffix_len = strlen(urc->cmd_suffix);
            if (bufsz < prefix_len + suffix_len) {
                continue;
            }
            if ((prefix_len ? !strncmp(buffer, urc->cmd_prefix, prefix_len) : 1) && (suffix_len ? !strncmp(buffer + bufsz - suffix_len, urc->cmd_suffix, suffix_len) : 1)) {
                return urc;
            }
        }
    }

    return NULL;
}

static int at_recv_readline(at_client_t client)
{
    size_t read_len = 0;
    char   ch = 0, last_ch = 0;
    bool   is_full = 0;

    memset(client->recv_line_buf, 0x00, client->recv_bufsz);
    client->recv_line_len = 0;

    while (1) {
        at_client_getchar(client, &ch, portMAX_DELAY);

        if (read_len < client->recv_bufsz) {
            client->recv_line_buf[read_len++] = ch;
            client->recv_line_len             = read_len;
            // NRF_LOG_RAW_INFO("%c\n", ch);
        } else {
            is_full = 1;
        }

        /* is newline or URC data */
        if ((ch == '\n' && last_ch == '\r') || (client->end_sign != 0 && ch == client->end_sign) || get_urc_obj(client)) {
            if (is_full) {
                NRF_LOG_ERROR("read line failed. The line data length is out of buffer size(%d)!", client->recv_bufsz);
                memset(client->recv_line_buf, 0x00, client->recv_bufsz);
                client->recv_line_len = 0;
                return -EFULL;
            }
            break;
        }
        last_ch = ch;
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("recvline", client->recv_line_buf, read_len);
#endif

    return read_len;
}

static void client_parser(at_client_t client)
{
    const struct at_urc* urc;

    while (1) {
        if (at_recv_readline(client) > 0) {
            if ((urc = get_urc_obj(client)) != NULL) {
                /* current receive is request, try to execute related operations */
                if (urc->func != NULL) {
                    urc->func(client, client->recv_line_buf, client->recv_line_len);
                }
            } else if (client->resp != NULL) {
                at_response_t resp = client->resp;

                char end_ch = client->recv_line_buf[client->recv_line_len - 1];

                /* current receive is response */
                client->recv_line_buf[client->recv_line_len - 1] = '\0';
                if (resp->buf_len + client->recv_line_len < resp->buf_size) {
                    /* copy response lines, separated by '\0' */
                    memcpy(resp->buf + resp->buf_len, client->recv_line_buf, client->recv_line_len);

                    /* update the current response information */
                    resp->buf_len += client->recv_line_len;
                    resp->line_counts++;
                } else {
                    client->resp_status = AT_RESP_BUFF_FULL;
                    NRF_LOG_ERROR("Read response buffer failed. The Response buffer size is out of buffer size(%d)!", resp->buf_size);
                }
                /* check response result */
                if ((client->end_sign != 0) && (end_ch == client->end_sign) && (resp->line_num == 0)) {
                    /* get the end sign, return response state END_OK.*/
                    client->resp_status = AT_RESP_OK;
                } else if (memcmp(client->recv_line_buf, AT_RESP_END_OK, strlen(AT_RESP_END_OK)) == 0 && resp->line_num == 0) {
                    /* get the end data by response result, return response state END_OK. */
                    client->resp_status = AT_RESP_OK;
                } else if (strstr(client->recv_line_buf, AT_RESP_END_ERROR) || (memcmp(client->recv_line_buf, AT_RESP_END_FAIL, strlen(AT_RESP_END_FAIL)) == 0)) {
                    client->resp_status = AT_RESP_ERROR;
                } else if (resp->line_counts == resp->line_num && resp->line_num) {
                    /* get the end data by response line, return response state END_OK.*/
                    client->resp_status = AT_RESP_OK;
                } else {
                    continue;
                }

                client->resp = NULL;

                xSemaphoreGive(client->resp_notice);
            } else {
                // NRF_LOG_DEBUG("unrecognized line: %*.s", client->recv_line_len, client->recv_line_buf);
            }
        }
    }
}

static void at_client_rx_ind(nrfx_uart_event_t const* p_event, void* p_context)
{
    taskDISABLE_INTERRUPTS();
    at_client_t client = (at_client_t)p_context;
    if (p_event->type == NRFX_UART_EVT_RX_DONE) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        nrfx_uart_rx(client->device, &rx_ch, 1);
        NRF_LOG_RAW_INFO("%c", rx_ch);
        xQueueSendFromISR(client->rx_queue, &rx_ch, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    if (p_event->type == NRFX_UART_EVT_TX_DONE) {
        transfer_status = true;
    }
    taskENABLE_INTERRUPTS();
}

/* initialize the client object parameters */
static int at_client_para_init(at_client_t client)
{
#define AT_CLIENT_LOCK_NAME   "at_c"
#define AT_CLIENT_SEM_NAME    "at_cs"
#define AT_CLIENT_RESP_NAME   "at_cr"
#define AT_CLIENT_THREAD_NAME "at_clnt"

    int        result        = EOK;
    static int at_client_num = 0;
    char       name[NAME_MAX];

    client->status = AT_STATUS_UNINITIALIZED;

    client->recv_line_len = 0;
    client->recv_line_buf = (char*)calloc(1, client->recv_bufsz);
    if (client->recv_line_buf == NULL) {
        NRF_LOG_ERROR("AT client initialize failed! No memory for receive buffer.");
        result = -ENOMEM;
        goto __exit;
    }

    client->rx_queue = xQueueCreate(64, sizeof(char*));
    if (client->rx_queue == NULL) {
        NRF_LOG_ERROR("AT client initialize failed! at_client_rx_queue create failed!");
        result = -ENOMEM;
        goto __exit;
    }

    client->lock = xSemaphoreCreateMutex();
    if (client->lock == NULL) {
        NRF_LOG_ERROR("AT client initialize failed! at_client_recv_lock create failed!");
        result = -ENOMEM;
        goto __exit;
    }

    client->resp_notice = xSemaphoreCreateCounting(UINT_FAST16_MAX, 0);
    if (client->resp_notice == NULL) {
        NRF_LOG_ERROR("AT client initialize failed! at_client_resp semaphore create failed!");
        result = -ENOMEM;
        goto __exit;
    }

    client->urc_table      = NULL;
    client->urc_table_size = 0;

    snprintf(name, NAME_MAX, "%s%d", AT_CLIENT_THREAD_NAME, at_client_num);
    xTaskCreate((void (*)(void* parameter))client_parser,
                name,
                512,
                client,
                tskIDLE_PRIORITY + 3,
                client->parser);

__exit:
    if (result != EOK) {
        if (client->lock) {
            vSemaphoreDelete(client->lock);
        }

        if (client->rx_queue) {
            vQueueDelete(client->rx_queue);
        }

        if (client->resp_notice) {
            vSemaphoreDelete(client->resp_notice);
        }

        if (client->device) {
            nrfx_uart_uninit(client->device);
        }

        if (client->recv_line_buf) {
            free(client->recv_line_buf);
        }

        memset(client, 0x00, sizeof(struct at_client));
    } else {
        at_client_num++;
    }

    return result;
}

/**
 * AT client initialize.
 *
 * @param dev_name AT client device name
 * @param recv_bufsz the maximum number of receive buffer length
 *
 * @return 0 : initialize success
 *        -1 : initialize failed
 *        -5 : no memory
 */
int at_client_init(const char* dev_name, size_t recv_bufsz)
{
    int         idx      = 0;
    int         result   = EOK;
    uint32_t    err_code = NRF_SUCCESS;
    at_client_t client   = NULL;

    ASSERT(recv_bufsz > 0);

    client             = &at_client_table[idx];
    client->recv_bufsz = recv_bufsz;

    result = at_client_para_init(client);
    if (result != EOK) {
        NRF_LOG_ERROR("AT client initialize para failed(%d).", result);
        goto __exit;
    }

    /* find and open command device */

    config.pseltxd   = UART_PIN_TX;
    config.pselrxd   = UART_PIN_RX;
    config.p_context = (void*)client;

    client->device = &p_instance;
    client->cfg    = &config;

    err_code = nrfx_uart_init(client->device, client->cfg, at_client_rx_ind);
    nrfx_uart_rx(client->device, &rx_ch, 1);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("AT client initialize uart failed(%d).", err_code);
        goto __exit;
    }

__exit:
    if (result == EOK) {
        client->status = AT_STATUS_INITIALIZED;

        vTaskResume(client->parser);

        NRF_LOG_INFO("AT client(V%s) on device %s initialize success.", AT_SW_VERSION, dev_name);
    } else {
        NRF_LOG_ERROR("AT client(V%s) on device %s initialize failed(%d).", AT_SW_VERSION, dev_name, result);
    }

    return result;
}
#endif /* AT_USING_CLIENT */

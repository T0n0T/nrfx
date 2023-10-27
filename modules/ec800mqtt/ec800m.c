/**
 * @file ec800m.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-26
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "ec800m.h"

#define NRF_LOG_MODULE_NAME ec800mqtt
#define NRF_LOG_LEVEL       NRF_LOG_SEVERITY_DEBUG
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static char send_buf[AT_CMD_BUF_LEN];
ec800m_t    ec800m;

/**
 * @brief
 *
 * @param dev
 * @param at_cmd_id
 * @param prase_here
 * @return int
 */

/**
 * @brief execute cmd with id
 *
 * @param dev get at_client instance
 * @param prase_buf if need to prase after the func,it should support by user
 * @param at_cmd_id located cmd from list
 * @param ... args used to combine with cmd
 * @return int
 */
int at_cmd_exec(at_client_t dev, char* prase_buf, at_cmd_desc_t at_cmd_id, ...)
{
    va_list       args;
    int           result        = EOK;
    const char*   recv_line_buf = 0;
    at_cmd_t      cmd           = (at_cmd_t)AT_CMD(at_cmd_id);
    at_response_t resp          = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, cmd->timeout);
    if (resp == NULL) {
        NRF_LOG_ERROR("No memory for response structure!");
        return -ENOMEM;
    }
    memset(send_buf, 0, sizeof(send_buf));

    va_start(args, at_cmd_id);
    vsprintf(send_buf, cmd->cmd_expr, args);
    va_end(args);

    result = at_obj_exec_cmd(dev, resp, send_buf);
    if (result < 0) {
        NRF_LOG_ERROR("AT client cmd [%d] timeout!", at_cmd_id);
        goto __exit;
    }

    if ((recv_line_buf = at_resp_get_line_by_kw(resp, cmd->resp_keyword)) == NULL) {
        NRF_LOG_ERROR("AT client execute [%d] failed!", at_cmd_id);
        goto __exit;
    }

    if (prase_buf) {
        strncpy(prase_buf, recv_line_buf, strlen(recv_line_buf));
    }

__exit:
    if (resp) {
        at_delete_resp(resp);
    }

    return result;
}

void ec800m_init_task(void* p)
{
    int result = EOK;
    NRF_LOG_DEBUG("Init at client device.");
    result = at_client_init("EC800MCN", AT_CLIENT_RECV_BUFF_LEN);
    if (result < 0) {
        NRF_LOG_ERROR("at client ec800m serial init failed.");
        return;
    }
    memset(&ec800m, 0, sizeof(ec800m));
    ec800m.client     = at_client_get(NULL);
    ec800m.pwr_pin    = EC800_PIN_PWR;
    ec800m.wakeup_pin = EC800_PIN_DTR;
    result            = at_client_obj_wait_connect(ec800m.client, 1000);
    if (result < 0) {
        NRF_LOG_ERROR("at client connect failed.");
        return;
    }
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_RESET);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m reset failed.");
        return;
    }
    vTaskDelay(500);
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_ECHO_OFF);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m echo set off failed.");
        return;
    }
    result = at_cmd_exec(ec800m.client, NULL, AT_CMD_CHECK_PIN);
    if (result < 0) {
        NRF_LOG_ERROR("ec800m echo set off failed.");
        return;
    }
    ec800m.is_init = true;
}

int ec800_init(void)
{

    static TaskHandle_t task_handle = NULL;

    BaseType_t xReturned = xTaskCreate(ec800m_init_task,
                                       "EC800M",
                                       512,
                                       0,
                                       5,
                                       &task_handle);
    if (xReturned != pdPASS) {
        NRF_LOG_ERROR("button task not created.");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
}
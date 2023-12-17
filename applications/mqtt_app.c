/**
 * @file mqtt_app.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-12-17
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "app.h"
#include "nrf_log.h"

void reconnect_handle(void* client, void* reconnect_date)
{
}

void sub_handle(void* client, message_data_t* msg)
{
    NRF_LOG_INFO("topic: %s\nmessage:", msg->topic_name);
    for (size_t i = 0; i < msg->message->payloadlen; i++) {
        NRF_LOG_RAW_INFO("%c", *((char*)msg->message->payload + i));
    }
}

void mqtt_conf(mqtt_client_t* client, config_t* cfg)
{
    mqtt_set_host(client, cfg->mqtt_cfg.host);
    mqtt_set_port(client, cfg->mqtt_cfg.port);
    if (cfg->mqtt_cfg.username && cfg->mqtt_cfg.password) {
        mqtt_set_user_name(client, cfg->mqtt_cfg.username);
        mqtt_set_password(client, cfg->mqtt_cfg.password);
    }
    mqtt_set_client_id(client, cfg->mqtt_cfg.clientid);
    if (mqtt_config.keepalive) {
        mqtt_set_keep_alive_interval(client, cfg->mqtt_cfg.keepalive);
    }
    mqtt_set_clean_session(client, 1);
    // mqtt_set_reconnect_data(client, NULL);
    // mqtt_set_reconnect_handler(client, reconnect_handle);
}

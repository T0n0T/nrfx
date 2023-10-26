#ifndef __AT_EC800_H__
#define __AT_EC800_H__

#include <at.h>
#include "bsp.h"
#include "rtdef.h"
#include "data.h"
#include "config_file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ec800_stat {
    EC800_STAT_INIT = 0,
    EC800_STAT_ATTACH,
    EC800_STAT_DEATTACH,
    EC800_STAT_CONNECTED,
    EC800_STAT_DISCONNECTED

} ec800_stat_t;

struct ec800_connect_obj {
    char host[100];
    char port[8];
};

struct ec800_device {
    uint32_t reset_pin;
    uint32_t adc_pin;
    ec800_stat_t stat;
    rt_mutex_t lock;
    rt_event_t event;
    rt_timer_t timer;
    uint8_t timer_count;
    char imei[16];
    char ipaddr[16];

    struct at_client *client;
    void (*parser)(const char *json);
};
typedef struct ec800_device *ec800_device_t;

/* NB-IoT */
int ec800_client_attach(void);
int ec800_client_deattach(void);

/* MQTT */
int ec800_mqtt_auth(void);
int ec800_mqtt_open(void);
int ec800_mqtt_close(void);
int ec800_mqtt_connect(void);
int ec800_mqtt_disconnect(void);
int ec800_mqtt_subscribe(const char *topic);
int ec800_mqtt_unsubscribe(const char *topic);
int ec800_mqtt_publish(const char *topic, const char *msg);
void ec800_bind_parser(void (*callback)(const char *json));

/* NB-IoT Network */
int ec800_init(void);
int ec800_build_mqtt_network(void);
int ec800_rebuild_mqtt_network(void);

int is_moudle_power_on(void);
int ec800_echo_off(void);
int ec800_subscribe_test(const char *topic);
int ec800_publish_test(const char *topic, const char *msg);

extern struct ec800_device ec800;
int ec800_gps_update(void);
int ec800_open_gps(void);
int ec800_update_location(struct mqtt_data *mqtt_report_data);
int mqtt_urc_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __AT_EC800_H__ */

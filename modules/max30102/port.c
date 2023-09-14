#include "max30102.h"

struct rt_sensor_config cfg = {
    .intf.dev_name = "i2c0",                 // i2c bus name
    .irq_pin.pin   = MAX_PIN_INT,            // interrupt pin
    .mode          = RT_SENSOR_MODE_POLLING, // must have
};

int max30102_port_init(void)
{

    if (rt_hw_max30102_init(&cfg) != RT_EOK)
        return RT_ERROR;

    return RT_EOK;
}
INIT_APP_EXPORT(max30102_port_init);
// MSH_CMD_EXPORT(max30102_port_init, test);

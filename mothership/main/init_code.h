#ifndef INIT_CODE_H
#define INIT_CODE_H

#include "driver/gptimer_types.h"
#include "i2cdev.h"

void timers_init(gptimer_alarm_cb_t timer_callback);

void init_comms();
void dac_init(i2c_dev_t *dev, uint8_t addr);

#endif // !DEBUG

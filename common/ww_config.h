#ifndef WW_CONFIG_H
#define WW_CONFIG_H

#define READ_SENSOR_INTERVAL_HZ (80 * 1)
#define TIMER_RES_FREQ_HZ       (16 * 1000)
#define TIMER_ALARM_COUNT       (TIMER_RES_FREQ_HZ / READ_SENSOR_INTERVAL_HZ)

// This value should be smaller than `ESPNOW_DATA_LEN = 218`
// Making this value smaller will increase the amount of transmissions the sensor does.
#define SAMPLES_BUFFER_SIZE (40)

#endif // !DEBUG

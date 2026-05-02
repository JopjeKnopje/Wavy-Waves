#ifndef WW_CONFIG_H
#define WW_CONFIG_H

#define READ_INTERVAL_HZ       (80 * 1)
#define READ_TIMER_RES_FREQ_HZ (16 * 1000)
#define READ_TIMER_ALARM_COUNT (READ_TIMER_RES_FREQ_HZ / READ_INTERVAL_HZ)

#define PLAYBACK_INTERVAL_HZ       (1 * READ_INTERVAL_HZ)
#define PLAYBACK_TIMER_RES_FREQ_HZ (16 * 1000)
#define PLAYBACK_TIMER_ALARM_COUNT (PLAYBACK_TIMER_RES_FREQ_HZ / PLAYBACK_INTERVAL_HZ)

// This value should be smaller than `ESPNOW_DATA_LEN = 218`
// Making this value smaller will increase the amount of transmissions the sensor does.
#define SAMPLES_BUFFER_SIZE (40)

#endif // !DEBUG

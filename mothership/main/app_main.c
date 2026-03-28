#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/gptimer_types.h"
#include "esp_err.h"
#include "freertos/task.h"
#include <freertos/FreeRTOS.h>

#include "hal/gpio_types.h"
#include "ww_config.h"

#include <mcp4725.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>

static const char *TAG = "esp32";

#define PIN_PULSE (33)

static bool status;

bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{

    gpio_set_level(PIN_PULSE, status);
    status = !status;
    return false;
}

void init_timers()
{

    static gptimer_handle_t gp_timer = NULL;
    static gptimer_config_t gp_timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        // frequency in which the timer should trigger.
        .resolution_hz = TIMER_RES_FREQ_HZ,
    };

    static gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        // amount of counts required before the alarm triggers.
        .alarm_count = TIMER_ALARM_COUNT,
        .flags.auto_reload_on_alarm = true,
    };

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_callback,
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&gp_timer_config, &gp_timer));

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gp_timer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gp_timer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_enable(gp_timer));
    ESP_ERROR_CHECK(gptimer_start(gp_timer));
}

void app_main()
{
    init_timers();
    gpio_set_direction(PIN_PULSE, GPIO_MODE_OUTPUT);
    status = 0;
    gpio_set_level(PIN_PULSE, status);
}

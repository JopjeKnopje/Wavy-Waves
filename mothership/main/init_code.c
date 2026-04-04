
#include "driver/gptimer.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "espnow.h"
#include "espnow_storage.h"
#include "mcp4725.h"
#include "ww_config.h"
#include <string.h>

void timers_init(gptimer_alarm_cb_t timer_callback)
{
    static gptimer_handle_t gp_timer = NULL;
    static gptimer_config_t gp_timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        // frequency in which the timer should trigger.
        .resolution_hz = PLAYBACK_TIMER_RES_FREQ_HZ,
    };

    static gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        // amount of counts required before the alarm triggers.
        .alarm_count = PLAYBACK_TIMER_ALARM_COUNT,
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

static void wait_for_eeprom(i2c_dev_t *dev)
{
    bool busy;
    while (true)
    {
        ESP_ERROR_CHECK(mcp4725_eeprom_busy(dev, &busy));
        if (!busy)
            return;
        printf("...DAC is busy, waiting...\n");
        vTaskDelay(1);
    }
}

void dac_init(i2c_dev_t *dev, uint8_t addr)
{
    ESP_ERROR_CHECK(i2cdev_init());

    memset(dev, 0, sizeof(i2c_dev_t));

    // Init device descriptor
    ESP_ERROR_CHECK(mcp4725_init_desc(dev, addr, 0, CONFIG_I2CDEV_DEFAULT_SDA_PIN, CONFIG_I2CDEV_DEFAULT_SCL_PIN));

    mcp4725_power_mode_t pm;
    ESP_ERROR_CHECK(mcp4725_get_power_mode(dev, true, &pm));
    if (pm != MCP4725_PM_NORMAL)
    {
        ESP_ERROR_CHECK(mcp4725_set_power_mode(dev, true, MCP4725_PM_NORMAL));
        wait_for_eeprom(dev);
    }

    // put DAC at half voltage
    ESP_ERROR_CHECK(mcp4725_set_raw_output(dev, 2047, true));
    wait_for_eeprom(dev);
}

static void app_wifi_init()
{
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void init_comms()
{
    espnow_storage_init();
    app_wifi_init();

    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_init(&espnow_config);

    // Get rid of broadcast peer, this is set by default in `espnow_init`.
    espnow_del_peer(ESPNOW_ADDR_BROADCAST);
}

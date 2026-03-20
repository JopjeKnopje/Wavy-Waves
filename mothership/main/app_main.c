#include "esp_log_level.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"

#include <mcp4725.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "espnow.h"
#include "espnow_storage.h"
#include "espnow_utils.h"

#define DAC_VDD (5)

#define DATA_OFFSET (0)

static const char *TAG = "mothership";

static i2c_dev_t dev;


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


void dac_init()
{
    ESP_ERROR_CHECK(i2cdev_init());

    memset(&dev, 0, sizeof(i2c_dev_t));

    // Init device descriptor
    ESP_ERROR_CHECK(mcp4725_init_desc(&dev, MCP4725A0_I2C_ADDR0, 0, CONFIG_I2CDEV_DEFAULT_SDA_PIN, CONFIG_I2CDEV_DEFAULT_SCL_PIN));

    mcp4725_power_mode_t pm;
    ESP_ERROR_CHECK(mcp4725_get_power_mode(&dev, true, &pm));
    if (pm != MCP4725_PM_NORMAL)
    {
		ESP_LOGI(TAG, "DAC was sleeping... Wake up Neo!\n");
        ESP_ERROR_CHECK(mcp4725_set_power_mode(&dev, true, MCP4725_PM_NORMAL));
        wait_for_eeprom(&dev);
    }

    ESP_ERROR_CHECK(mcp4725_set_raw_output(&dev, 0, true));
    wait_for_eeprom(&dev);
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

static esp_err_t receive_handle(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    static uint32_t count = 0;

	const uint32_t value = *(uint32_t *)data;
    ESP_LOGI(TAG, "espnow_recv, <%" PRIu32 "> [" MACSTR "][%d][%d][%u]: %u", count++, MAC2STR(src_addr), rx_ctrl->channel, rx_ctrl->rssi, size,
             value);

	// HAHAH WTF
	// TODO: Implement some kind of DSP thingy here where it actually takes the average.
	const uint32_t center = 2048;
	uint16_t dac_value = ((value / 1000) - 26000 + center);

	ESP_LOGI(TAG, "wiritng to DAC %u", dac_value);
    ESP_ERROR_CHECK(mcp4725_set_raw_output(&dev, dac_value, false));


    return ESP_OK;
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


void app_main()
{
	dac_init();
    init_comms();

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, receive_handle);
}

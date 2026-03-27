#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/gptimer_types.h"
#include "esp_err.h"
#include "esp_log_level.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "portmacro.h"
#include "samples.h"
#include "ww_config.h"

#include <mcp4725.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/unistd.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "espnow.h"
#include "espnow_storage.h"
#include "espnow_utils.h"

#define DAC_VDD (5)

#define DATA_OFFSET (0)

#define PIN_PULSE         (33)
#define QUEUE_SAMPLES_LEN (8)

static const char *TAG = "mothership";

static i2c_dev_t dev;

static double_samples_t double_samples;

static QueueHandle_t queue_samples;

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
        // ESP_LOGI(TAG, "DAC was sleeping... Wake up Neo!\n");
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

void init_comms()
{
    espnow_storage_init();
    app_wifi_init();

    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_init(&espnow_config);

    // Get rid of broadcast peer, this is set by default in `espnow_init`.
    espnow_del_peer(ESPNOW_ADDR_BROADCAST);
}

static esp_err_t receive_handle(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    static size_t count = 0;
    const samples_t samples = *(samples_t *)data;
    ESP_LOGI(TAG, "espnow_recv, <%" PRIu32 "> [" MACSTR "][%d][%d][%u]: %u", count++, MAC2STR(src_addr), rx_ctrl->channel, rx_ctrl->rssi, size,
             samples.s_data[0]);

    if (xQueueSend(queue_samples, samples.s_data, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "failed adding samples to queue");
    }

    return ESP_OK;
}

static void task_write_dac()
{

    while (1)
    {
        samples_t samples;
        if (xQueueReceive(queue_samples, &samples, 8) != pdPASS)
        {
            ESP_LOGE(TAG, "failed getting sample from queue");
            continue;
        }

        size_t index = 0;
        while (index < SAMPLES_BUFFER_SIZE)
        {
            uint16_t dac_value = samples.s_data[index];
            printf("%u\n", dac_value);
            ESP_ERROR_CHECK(mcp4725_set_raw_output(&dev, dac_value, false));
            index++;
            vTaskDelay(pdMS_TO_TICKS(13));
        }

        ESP_LOGW(TAG, "after delay");
    }
}

void app_main()
{
    dac_init();
    init_comms();
    dsample_init(&double_samples);

    queue_samples = xQueueCreate(QUEUE_SAMPLES_LEN, sizeof(uint16_t) * SAMPLES_BUFFER_SIZE);
    if (queue_samples == NULL)
    {
        ESP_LOGE(TAG, "<%s> failed creating queue");
    }

    // this espnow stuff runs on core 1, so we do our processing on core 0
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, receive_handle);

    xTaskCreatePinnedToCore(task_write_dac, "write_dac", 4 * 1024, NULL, 1, 0, 0);
}

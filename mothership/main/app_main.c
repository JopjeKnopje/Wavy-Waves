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
#include "hal/gpio_types.h"
#include "ping/ping_sock.h"
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
#define QUEUE_SAMPLES_LEN (1)

static const char *TAG = "mothership";

static i2c_dev_t dev;

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
    gpio_set_level(PIN_PULSE, 1);

    static size_t count = 0;
    samples_t samples = *(samples_t *)data;
    ESP_LOGI(TAG, "espnow_recv, <%" PRIu32 "> [" MACSTR "][%d][%d][%u]: %u", count++, MAC2STR(src_addr), rx_ctrl->channel, rx_ctrl->rssi, size,
             samples.s_data[0]);

    // reset sample index
    // samples.index = 0;
    if (xQueueSend(queue_samples, samples.s_data, portMAX_DELAY) != pdPASS)
    {
        ESP_LOGE(TAG, "failed adding samples to queue");
    }
    gpio_set_level(PIN_PULSE, 0);

    return ESP_OK;
}

static TaskHandle_t th_write_dac;

static void task_write_dac()
{
    bool finished_buffer;

    samples_t samples;
    size_t index = 0;

    while (1)
    {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        finished_buffer = index >= SAMPLES_BUFFER_SIZE;
        if (finished_buffer)
        {
            index = 0;
            if (xQueueReceive(queue_samples, &samples, 0) != pdPASS)
            {
                ESP_LOGE(TAG, "failed getting sample from queue");
                continue;
            }
        }

        if (!finished_buffer)
        {
            uint16_t dac_value = samples.s_data[index];
            printf("%u\n", dac_value);
            ESP_ERROR_CHECK(mcp4725_set_raw_output(&dev, dac_value, false));
            index++;
            portYIELD();
        }
    }
    vTaskDelete(NULL);
}

bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    // TODO: should this be static?
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // instead of fire task, we just step through buffer here.
    vTaskNotifyGiveFromISR(th_write_dac, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

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
    dac_init();
    init_comms();
    init_timers();

    gpio_set_direction(PIN_PULSE, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_PULSE, 0);

    queue_samples = xQueueCreate(QUEUE_SAMPLES_LEN, sizeof(uint16_t) * SAMPLES_BUFFER_SIZE);
    if (queue_samples == NULL)
    {
        ESP_LOGE(TAG, "<%s> failed creating queue");
    }

    // this espnow stuff runs on core 1, so we do our processing on core 0
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, receive_handle);

    xTaskCreatePinnedToCore(task_write_dac, "write_dac", 4 * 1024, NULL, configMAX_PRIORITIES - 1, &th_write_dac, 0);
}

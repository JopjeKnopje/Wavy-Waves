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
#define QUEUE_SAMPLES_LEN (8)

static const char *TAG = "mothership";

static i2c_dev_t dev;

static QueueHandle_t q_playback;

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

typedef struct
{
    uint16_t s_data[SAMPLES_BUFFER_SIZE];
    uint16_t playback_time;
} timed_playback_t;

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

    static TickType_t old_tick = 0;
    if (old_tick == 0)
        old_tick = xTaskGetTickCount();

    // take measurement of current received packet.
    const TickType_t cur_tick = xTaskGetTickCount();
    // get delta between old recieve time and current.
    const TickType_t delta_tick = cur_tick - old_tick;
    old_tick = cur_tick;

    ESP_LOGI(TAG, "ticks: %u", delta_tick);

    // TODO: Rename this variable lol wtf.
    samples.index = delta_tick;
    if (xQueueSend(q_playback, samples.s_data, portMAX_DELAY) != pdPASS)
    {
        ESP_LOGE(TAG, "failed adding samples to queue");
    }
    gpio_set_level(PIN_PULSE, 0);

    return ESP_OK;
}

static TaskHandle_t th_write_dac;

static void task_write_dac()
{

    timed_playback_t playback;
    size_t index = 0;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (index >= SAMPLES_BUFFER_SIZE)
        {
            index = 0;
            if (xQueueReceive(q_playback, &playback, 0) != pdPASS)
            {
                ESP_LOGE(TAG, "failed getting sample from queue");
                continue;
            }
        }

        // TODO: Add offset to config
        uint16_t dac_value = (playback.s_data[index] - 10180) * 40;
        ESP_LOGI(TAG, "before: %u, after: %u", playback.s_data[index], dac_value);
        ESP_ERROR_CHECK(mcp4725_set_raw_output(&dev, dac_value, false));
        index++;
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

    gpio_set_direction(PIN_PULSE, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_PULSE, 0);

    q_playback = xQueueCreate(QUEUE_SAMPLES_LEN, sizeof(timed_playback_t));
    if (q_playback == NULL)
    {
        ESP_LOGE(TAG, "<%s> failed creating queue");
    }

    // this espnow stuff runs on core 1, so we do our processing on core 0
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, receive_handle);

    init_timers();

    xTaskCreatePinnedToCore(task_write_dac, "write_dac", 4 * 1024, NULL, configMAX_PRIORITIES - 1, &th_write_dac, 0);
}

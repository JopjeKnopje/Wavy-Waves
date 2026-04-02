#include "FreeRTOSConfig.h"
#include "bmp280.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/gptimer_types.h"
#include "esp_err.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/unistd.h>

#include "i2cdev.h"
#include "portmacro.h"
#include "samples.h"
#include "ww_config.h"
#include "ww_data.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#define QUEUE_PRESSURE_LEN (32)

#include "espnow.h"
#include "espnow_storage.h"
#include "espnow_utils.h"

static const char *TAG = "sensor";

static QueueHandle_t queue_samples;

static TaskHandle_t th_read_sensor;

static bmp280_t sensor;

#define PIN_PULSE 33

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

void send_message()
{
    espnow_frame_head_t frame_head = {
        .retransmit_count = CONFIG_RETRY_NUM,
        .broadcast = false,
        .ack = true,
    };

    samples_t samples;
    samples_init(&samples);

    while (1)
    {
        uint16_t data;
        if (xQueueReceive(queue_samples, &data, 8) != pdPASS)
        {
            ESP_LOGW(TAG, "no messages in queue");
            continue;
        }

        // printf("%u\n", data);
        const size_t free_spaces = samples_add(&samples, data);
        if (free_spaces)
        {
            continue;
        }

        while (1)
        {
            esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, MOTHERSHIP_MAC, &samples.s_data, sizeof(sample_buffer_t), &frame_head, portMAX_DELAY);
            if (ret != ESP_OK)
                ESP_LOGE(TAG, "<%s> espnow_send", esp_err_to_name(ret));
            else
            {
                // ESP_LOGI(TAG, "espnow_send, data: %u", samples.s_data[0]);
                break;
            }
        }
        samples_reset(&samples);
    }
    vTaskDelete(NULL);
}

static esp_err_t receive_handle(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    static uint32_t count = 0;

    ESP_LOGI(TAG, "espnow_recv, <%" PRIu32 "> [" MACSTR "][%d][%d][%u]: %f", count++, MAC2STR(src_addr), rx_ctrl->channel, rx_ctrl->rssi, size, size,
             *(uint32_t *)data);

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
    espnow_add_peer(MOTHERSHIP_MAC, NULL);

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, receive_handle);
}

bmp280_t init_sensor()
{
    ESP_ERROR_CHECK(i2cdev_init());

    bmp280_params_t params;
    bmp280_init_default_params(&params);
    params.standby = BMP280_STANDBY_05;
    memset(&sensor, 0, sizeof(bmp280_t));

    ESP_ERROR_CHECK(bmp280_init_desc(&sensor, BMP280_I2C_ADDRESS_0, 0, CONFIG_I2CDEV_DEFAULT_SDA_PIN, CONFIG_I2CDEV_DEFAULT_SCL_PIN));
    ESP_ERROR_CHECK(bmp280_init(&sensor, &params));

    return sensor;
}

void read_sensor(void *data)
{
    float pressure;
    float y;
    float x;

    size_t value = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (bmp280_read_float(&sensor, &x, &pressure, &y) != ESP_OK)
        {
            ESP_LOGE(TAG, "bmp280 read failed");
            continue;
        }

        // uint16_t data = ((uint32_t)pressure) / 10;
        uint16_t data = value;

        // TODO: use `ESP_ERROR_CHECK`?
        // Don't wait for the queue to be avaliable, if we do wait for any amount of time here. It will throw off `READ_SENSOR_INTERVAL_HZ`
        if (xQueueSend(queue_samples, &data, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "failed adding data [%u] to queue", pressure);
        }
        value += 8;
        value %= 4095;
    }
    vTaskDelete(NULL);
}

// TODO: should this be IRAM_ATTR?
bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    // TODO: should this be static?
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveFromISR(th_read_sensor, &xHigherPriorityTaskWoken);

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
        .resolution_hz = READ_TIMER_RES_FREQ_HZ,
    };

    static gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        // amount of counts required before the alarm triggers.
        .alarm_count = READ_TIMER_ALARM_COUNT,
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
    queue_samples = xQueueCreate(QUEUE_PRESSURE_LEN, sizeof(uint16_t));
    if (queue_samples == NULL)
    {
        ESP_LOGE(TAG, "<%s> failed creating queue");
    }

    gpio_set_direction(PIN_PULSE, GPIO_MODE_OUTPUT);

    init_comms();
    init_sensor();

    init_timers();

    xTaskCreatePinnedToCore(read_sensor, "read_sensor", 4 * 1024, NULL, configMAX_PRIORITIES - 1, &th_read_sensor, 0);
    xTaskCreatePinnedToCore(send_message, "send_message", 4 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL, 1);

    vTaskDelete(NULL);
}

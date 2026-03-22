#include "FreeRTOSConfig.h"
#include "bmp280.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log_level.h"
#include "esp_now.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>

#include "hal/timer_types.h"
#include "i2cdev.h"
#include "lwip/pbuf.h"
#include "portmacro.h"
#include "soc/clk_tree_defs.h"
#include "ww_data.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "espnow.h"
#include "espnow_storage.h"
#include "espnow_utils.h"

static const char *TAG = "sensor";

static QueueHandle_t queue_sensor;

#define QUEUE_PRESSURE_LEN (8)

static TaskHandle_t th_send_message;

static bmp280_t sensor;

#define READ_SENSOR_TASKDELAY (10)

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
    const size_t size = sizeof(uint32_t);

    esp_err_t ret = ESP_OK;

    espnow_frame_head_t frame_head = {
        .retransmit_count = CONFIG_RETRY_NUM,
        .broadcast = false,
        .ack = true,
    };

    while (1)
    {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "send_message notified");

        while (1)
        {
            uint32_t data;
            if (xQueueReceive(queue_sensor, &data, 10) != pdPASS)
            {
                ESP_LOGW(TAG, "no messages in queue");
                break;
            }

            ret = espnow_send(ESPNOW_DATA_TYPE_DATA, MOTHERSHIP_MAC, &data, size, &frame_head, portMAX_DELAY);
            if (ret != ESP_OK)
                ESP_LOGE(TAG, "<%s> espnow_send", esp_err_to_name(ret));

            ESP_LOGI(TAG, "espnow_send, size: %u, data: %u", size, data);
        }
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
    memset(&sensor, 0, sizeof(bmp280_t));

    ESP_ERROR_CHECK(bmp280_init_desc(&sensor, BMP280_I2C_ADDRESS_0, 0, CONFIG_I2CDEV_DEFAULT_SDA_PIN, CONFIG_I2CDEV_DEFAULT_SCL_PIN));
    ESP_ERROR_CHECK(bmp280_init(&sensor, &params));

    return sensor;
}

typedef void(t_data_ready_cb)(float data);

void read_sensor(void *data)
{
    t_data_ready_cb *const cb = data;

    float pressure;
    float y;
    float x;

    while (1)
    {
        if (bmp280_read_float(&sensor, &x, &pressure, &y) != ESP_OK)
        {
            ESP_LOGE(TAG, "bmp280 read failed");
            continue;
        }

        uint32_t data = (uint32_t)pressure;
        if (xQueueSend(queue_sensor, &data, 10) != pdPASS)
        {
            ESP_LOGE(TAG, "failed adding data [%u] to queue", pressure);
        }
        else
        {
            cb(pressure);
        }

        vTaskDelay(pdMS_TO_TICKS(READ_SENSOR_TASKDELAY));
    }
    vTaskDelete(NULL);
}

// TODO: Look into nicer way to do typedef for callbacks. (LVGL for example)
void data_ready_cb(float x)
{
    const uint32_t queue_size = QUEUE_PRESSURE_LEN - uxQueueSpacesAvailable(queue_sensor);
    ESP_LOGI(TAG, "queue len %d", queue_size);
    if (queue_size > 0)
    {
        xTaskNotifyGive(th_send_message);
    }
}

#define PIN_PULSE 33

static SemaphoreHandle_t timer_sem;
static TaskHandle_t th_timed_task;

bool IRAM_ATTR example_timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // if we caused a higher priority task to unblock, we should yield from this ISR and let that task run.
    // xSemaphoreGiveFromISR(timer_sem, &xHigherPriorityTaskWoken);

    vTaskNotifyGiveFromISR(th_timed_task, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return false;
}

static gptimer_handle_t gp_timer = NULL;
static gptimer_config_t gp_timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    // set the pre-scaler.
    .resolution_hz = 2 * 1000 // 2 Khz
};

static gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,
    .alarm_count = 1,
    .flags.auto_reload_on_alarm = true,
};

gptimer_event_callbacks_t cbs = {
    .on_alarm = example_timer_on_alarm_cb, // Call the user callback function when the alarm event occurs
};

void timed_task()
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI("timed_task", "hello");
    }
}

void app_main()
{

    timer_sem = xSemaphoreCreateBinary();
    gpio_set_direction(PIN_PULSE, GPIO_MODE_OUTPUT);
    ESP_ERROR_CHECK(gptimer_new_timer(&gp_timer_config, &gp_timer));

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gp_timer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gp_timer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_enable(gp_timer));
    ESP_ERROR_CHECK(gptimer_start(gp_timer));

    xTaskCreatePinnedToCore(timed_task, "timed_task", 4 * 1024, NULL, configMAX_PRIORITIES - 1, &th_timed_task, 0);

    // queue_sensor = xQueueCreate(QUEUE_PRESSURE_LEN, sizeof(uint32_t));
    // if (queue_sensor == NULL)
    // {
    //     ESP_LOGE(TAG, "<%s> failed creating queue");
    // }
    //
    // init_sensor();
    // init_comms();
    //
    // xTaskCreatePinnedToCore(read_sensor, "read_sensor", 4 * 1024, data_ready_cb, tskIDLE_PRIORITY + 1, NULL, 0);
    // xTaskCreatePinnedToCore(send_message, "send_message", 4 * 1024, NULL, tskIDLE_PRIORITY + 1, &th_send_message, 1);
}

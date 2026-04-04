#include "driver/gptimer_types.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "init_code.h"

#include "esp_log.h"
#include "portmacro.h"
#include "samples.h"
#include "ww_config.h"
#include "ww_data.h"

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
#include "espnow_utils.h"

#define DAC_VDD (5)

#define DATA_OFFSET (0)

#define PIN_PULSE         (33)
#define QUEUE_SAMPLES_LEN (32)

static const char *TAG = "mothership";

typedef enum
{
    DAC_0 = 0,
    DAC_1 = 1,
    DAC_MAX = 2,
} dac_index_t;

typedef struct
{
    sample_buffer_t s_data;
    uint8_t s_index;
} playback_t;

static void playback_init(playback_t *p, void *data)
{
    ESP_ERROR_CHECK(!(memcpy(p->s_data, data, sizeof(sample_buffer_t)) == p->s_data));
    p->s_index = 0;
}

typedef struct
{
    QueueHandle_t queue;
    i2c_dev_t dev_handle;
    dac_index_t id;
} dac_writer_handle_t;

static dac_writer_handle_t dacs[DAC_MAX];
static TaskHandle_t th_dacs[DAC_MAX];

void dac_writer_init(dac_writer_handle_t *dh, dac_index_t id, uint8_t dac_addr)
{
    dh->id = id;
    dac_init(&dh->dev_handle, dac_addr);
    dh->queue = xQueueCreate(QUEUE_SAMPLES_LEN, sizeof(playback_t));
    ESP_ERROR_CHECK(dh->queue == NULL);
}

static esp_err_t receive_handle(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    static size_t count = 0;
    playback_t playback;

    playback_init(&playback, data);

    ESP_LOGI(TAG, "espnow_recv, <%" PRIu32 "> [" MACSTR "][%d][%d][%u]: %u", count++, MAC2STR(src_addr), rx_ctrl->channel, rx_ctrl->rssi, size,
             playback.s_data[0]);

    // TODO: Have list of DACs and assign based on availability at startup.
    // Have map[MAC_ADDR, DAC_INDEX] to dynamically map DACs.

    dac_index_t dac_index;

    bool is_from_dev_6 = memcmp(src_addr, DEV_6_MAC, sizeof(espnow_addr_t)) == 0;
    if (is_from_dev_6)
        dac_index = DAC_0;
    else
        dac_index = DAC_1;

    if (xQueueSend(dacs[dac_index].queue, &playback, portMAX_DELAY) != pdPASS)
    {
        ESP_LOGE(TAG, "DAC: [%d] failed adding samples to queue", dac_index);
    }

    return ESP_OK;
}

static bool receive_playback_from_queue(playback_t *playback, QueueHandle_t pbq, dac_index_t dac_id)
{

    if (playback->s_index >= SAMPLES_BUFFER_SIZE)
    {
        if (xQueueReceive(pbq, playback, 0) != pdPASS)
            return false;
        playback->s_index = 0;
    }
    return true;
}

static void task_write_dac(void *param)
{
    const static uint8_t ERROR_COUNT = 3;

    dac_writer_handle_t *dac_handle = (dac_writer_handle_t *)param;
    playback_t playback;

    size_t errors = 0;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        bool success = receive_playback_from_queue(&playback, dac_handle->queue, dac_handle->id);
        if (!success)
        {
            ESP_LOGE(TAG, "DAC: [%d] queue empty", dac_handle->id);
            errors++;
        }
        else
            errors = 0;

        if (errors > ERROR_COUNT)
        {
            ESP_LOGE(TAG, "DAC: [%d] sensor offline?", dac_handle->id);
            // dirty delay to fix getting stuck in error state when both sensors start transmitting again.
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        uint16_t dac_value = (playback.s_data[playback.s_index] - 10160) * 40;
        // uint16_t dac_value = playback.s_data[playback.s_index];

        // TODO: Set eeprom to true?
        // ESP_LOGI(TAG, "dac index : %d", dac_handle->id);
        ESP_ERROR_CHECK(mcp4725_set_raw_output(&dac_handle->dev_handle, dac_value, false));
        playback.s_index++;
    }
    vTaskDelete(NULL);
}

bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    // TODO: should this be static?
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static dac_index_t dac_index = DAC_0;

    dac_index = !dac_index;
    vTaskNotifyGiveFromISR(th_dacs[dac_index], &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return false;
}

void app_main()
{
    dac_writer_init(&dacs[DAC_0], DAC_0, MCP4725A0_I2C_ADDR0);
    dac_writer_init(&dacs[DAC_1], DAC_1, MCP4725A0_I2C_ADDR1);

    init_comms();

    // this espnow stuff runs on core 1, so we do our processing on core 0
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, receive_handle);

    xTaskCreatePinnedToCore(task_write_dac, "write_dac_0", 4 * 1024, &dacs[DAC_0], configMAX_PRIORITIES - 1, &th_dacs[DAC_0], 0);
    xTaskCreatePinnedToCore(task_write_dac, "write_dac_1", 4 * 1024, &dacs[DAC_1], configMAX_PRIORITIES - 1, &th_dacs[DAC_1], 0);

    // TODO: Put timer code in timer.c file, we can pass along the timer `FREQ` and `COUNT`
    timers_init(timer_callback);
}

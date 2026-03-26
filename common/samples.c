#include "samples.h"
#include "esp_err.h"
#include <string.h>

void samples_init(samples_t *samples)
{
    ESP_ERROR_CHECK(samples == NULL);
    memset(samples, 0, sizeof(samples_t));
}

void samples_reset(samples_t *samples) { samples_init(samples); }

size_t samples_length(samples_t *samples)
{
    ESP_ERROR_CHECK(samples == NULL);
    return samples->index;
}

size_t samples_add(samples_t *samples, uint32_t data)
{
    ESP_ERROR_CHECK(samples == NULL);
    ESP_ERROR_CHECK(samples->index >= SAMPLES_BUFFER_SIZE);

    samples->samples[samples->index] = data;
    samples->index++;

    return samples_capacity(samples);
}

size_t samples_capacity(samples_t *samples) { return SAMPLES_BUFFER_SIZE - samples_length(samples); }

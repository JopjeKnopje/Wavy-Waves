#include "samples.h"
#include "esp_err.h"
#include <string.h>
#include <sys/types.h>

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

size_t samples_add(samples_t *samples, uint16_t data)
{
    ESP_ERROR_CHECK(samples == NULL);
    ESP_ERROR_CHECK(samples->index >= SAMPLES_BUFFER_SIZE);

    samples->samples[samples->index] = data;
    samples->index++;

    return samples_capacity(samples);
}

size_t samples_capacity(samples_t *samples) { return SAMPLES_BUFFER_SIZE - samples_length(samples); }

void dsample_init(double_samples_t *ds)
{
    ESP_ERROR_CHECK(ds == NULL);
    memset(ds, 0, sizeof(double_samples_t));
    ds->handle = &ds->buf[DS_SAMPLES_FIRST];
}

static samples_t *dsample_get_non_active_handle(double_samples_t *const ds)
{
    samples_t *handle;
    if (ds->handle == &ds->buf[DS_SAMPLES_FIRST])
        handle = &ds->buf[DS_SAMPLES_SECOND];
    else
        handle = &ds->buf[DS_SAMPLES_FIRST];
    return handle;
}

void dsample_swap(double_samples_t *ds)
{
    ESP_ERROR_CHECK(ds == NULL);
    ds->handle = dsample_get_non_active_handle(ds);
}

void dsample_set_samples(double_samples_t *ds, const samples_t *s) { memcpy(ds->handle, s->samples, sizeof(uint16_t) * SAMPLES_BUFFER_SIZE); }

void dsample_get_samples(double_samples_t *ds, samples_t *s)
{
    // get the non active sample buffer.
    const samples_t *handle = dsample_get_non_active_handle(ds);
    memcpy(s, handle, sizeof(uint16_t) * SAMPLES_BUFFER_SIZE);
    // set flag that new buffer is ready for new data.
}

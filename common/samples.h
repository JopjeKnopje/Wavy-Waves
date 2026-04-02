#ifndef SAMPLES_H
#define SAMPLES_H

#include "ww_config.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

typedef uint16_t sample_buffer_t[SAMPLES_BUFFER_SIZE];

typedef struct
{
    sample_buffer_t s_data;
    size_t index;
} samples_t;

void samples_init(samples_t *samples);
void samples_reset(samples_t *samples);
size_t samples_length(samples_t *samples);
size_t samples_add(samples_t *samples, uint16_t data);
size_t samples_capacity(samples_t *samples);

typedef enum
{
    DS_SAMPLES_FIRST = 0,
    DS_SAMPLES_SECOND = 1,
    DS_SAMPLES_MAX = 2,
} double_samples_buffer_index_t;

// will contain 2 samples, one which we are currently sending out and another one which is being received
// we will only start writing the first buffer when we've received the next one.
// we will spread out the playing of the first buffer based on the time it took to recieve the second one.
typedef struct
{
    samples_t buf[DS_SAMPLES_MAX];
    samples_t *handle;

} double_samples_t;

void dsample_init(double_samples_t *ds);
void dsample_swap(double_samples_t *ds);
void dsample_copy_samples(double_samples_t *ds, samples_t *s);
samples_t *dsample_get_sample_handle(double_samples_t *ds);
samples_t *dsample_get_non_active_handle(double_samples_t *const ds);
void dsample_set_samples(double_samples_t *ds, const samples_t *s);

#endif

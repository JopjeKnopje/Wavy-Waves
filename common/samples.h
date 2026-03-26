#ifndef SAMPLES_H
#define SAMPLES_H

#include <stddef.h>
#include <stdint.h>

#define SAMPLES_BUFFER_SIZE (40)

typedef struct
{
    uint16_t samples[SAMPLES_BUFFER_SIZE];
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
} double_samples_buffer_t;

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
void dsample_get_samples(double_samples_t *ds, samples_t *s);
void dsample_set_samples(double_samples_t *ds, const samples_t *s);

#endif

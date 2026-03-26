#ifndef SAMPLES_H
#define SAMPLES_H

#include <stddef.h>
#include <stdint.h>

#define SAMPLES_BUFFER_SIZE (40)

typedef struct
{
    uint32_t samples[SAMPLES_BUFFER_SIZE];
    size_t index;
} samples_t;

void samples_init(samples_t *samples);
void samples_reset(samples_t *samples);
size_t samples_length(samples_t *samples);
size_t samples_add(samples_t *samples, uint32_t data);
size_t samples_capacity(samples_t *samples);

#endif

#include "samples.h"
#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define BIT(pos) (1 << pos)

void setUp(void) {}

void tearDown(void) {}

void populate_samples(samples_t *s, uint16_t start)
{
    size_t i = start;

    while (1)
    {
        const size_t free_spaces = samples_add(s, i);

        if (!free_spaces)
            break;
        i++;
    }
}

void test_set_samples()
{
    samples_t s;
    samples_init(&s);
    populate_samples(&s, 0);

    double_samples_t ds;
    dsample_init(&ds);

    samples_t *ds_current_handle = ds.handle;
    dsample_set_samples(&ds, &s);
    // make sure we actually set the samples
    TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(s.s_data, dsample_get_sample_handle(&ds)->s_data, SAMPLES_BUFFER_SIZE, "samples don't match");
}

void test_sd()
{
    samples_t sample_1;
    samples_init(&sample_1);
    populate_samples(&sample_1, 0);

    samples_t sample_2;
    samples_init(&sample_2);
    populate_samples(&sample_2, SAMPLES_BUFFER_SIZE);

    double_samples_t ds;
    dsample_init(&ds);

    dsample_set_samples(&ds, &sample_1);
    // make sure we actually set the samples
    TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(sample_1.s_data, dsample_get_sample_handle(&ds)->s_data, SAMPLES_BUFFER_SIZE, "samples don't match");

    const samples_t *ds_prev_handle = dsample_get_sample_handle(&ds);
    dsample_swap(&ds);

    TEST_ASSERT_NOT_EQUAL(dsample_get_sample_handle(&ds), ds_prev_handle);
    TEST_ASSERT_EACH_EQUAL_UINT16_MESSAGE(0, dsample_get_sample_handle(&ds)->s_data, SAMPLES_BUFFER_SIZE, "samples should be 0");

    // write sample into second buffer
    dsample_set_samples(&ds, &sample_2);
    TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(sample_2.s_data, dsample_get_sample_handle(&ds)->s_data, SAMPLES_BUFFER_SIZE, "samples don't match");
}



#include "driver/gpio.h"

#define LED_STATUS_PIN (2)

static bool initialized = false;

void _led_set(bool level) { gpio_set_level(LED_STATUS_PIN, level); }

void led_init(void)
{
    gpio_reset_pin(LED_STATUS_PIN);
    gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);
}

void led_post_init() { _led_set(0); }

void led_set_error()
{
    if (!initialized)
        _led_set(1);
}

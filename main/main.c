#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#define BLINK_GPIO  8

void app_main(void)
{
    BaseType_t delayTicks = 2000 / portTICK_PERIOD_MS;

    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    uint8_t led = 0;

    while(1)
    {
        led = !led;
        gpio_set_level(BLINK_GPIO, led);
        vTaskDelay(delayTicks);
    }
}
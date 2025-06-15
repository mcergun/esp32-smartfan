#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "dht.h"

#define LOG_TAG "FANCTL"

#define BLINK_GPIO          GPIO_NUM_8
#define DHT22_GPIO          GPIO_NUM_10
#define DHT22_SENSOR_TYPE   DHT_TYPE_AM2301

void app_main(void)
{
    const TickType_t delayTicks = 2000 / portTICK_PERIOD_MS;

    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    uint8_t led = 0;
    int16_t humidity;
    int16_t temperature;
    esp_err_t ret;

    while(1)
    {
        led = !led;
        gpio_set_level(BLINK_GPIO, led);
        ret = dht_read_data(DHT22_SENSOR_TYPE, DHT22_GPIO, &humidity, &temperature);
        if (ret == ESP_OK)
        {
            ESP_LOGI(LOG_TAG, "Temperature = %d, Humidity = %d", temperature, humidity);
        }
        vTaskDelay(delayTicks);
    }
}
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

#include "dht.h"

#define LOG_TAG "FANCTL"

#define BLINK_GPIO          GPIO_NUM_8
#define DHT22_GPIO          GPIO_NUM_10
#define DHT22_SENSOR_TYPE   DHT_TYPE_AM2301

#define FAN_PWM_FREQ        (25 * 1000)
#define FAN_PWM_TIMER       LEDC_TIMER_0
#define FAN_PWM_SPEED       LEDC_LOW_SPEED_MODE
#define FAN_PWM_CHANNEL     LEDC_CHANNEL_0
#define FAN_PWM_DUTY_RES    LEDC_TIMER_10_BIT
#define FAN_PWM_DUTY_MAX    ((0x1u << ((size_t)FAN_PWM_DUTY_RES)) - 1)
#define FAN_GPIO            GPIO_NUM_8

esp_err_t InitializePwmController(void);
esp_err_t InitializeNvs(void);

typedef struct fan_control_state
{
    /// @brief minimum humidity value * 10 for fan to start working
    int16_t humidty_min;
    /// @brief maximum humidty value * 10 for max fan cycles
    int16_t humidity_max;
    /// @brief granularity of fan control steps
    int16_t steps;
    /// @brief current duty of fan
    uint32_t duty;
} fan_control_state_t;

fan_control_state_t fan = {
    .humidty_min = 30 * 10,
    .humidity_max = 70 * 10,
    .steps = (70 - 30) * 10,
    .duty = 0,
};

esp_err_t InitializePwmController(void)
{
    ledc_timer_config_t pwmTimer = {
        .speed_mode = FAN_PWM_SPEED,
        .duty_resolution = FAN_PWM_DUTY_RES,
        .timer_num = FAN_PWM_TIMER,
        .freq_hz = FAN_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ledc_channel_config_t pwmChannel = {
        .gpio_num = FAN_GPIO,
        .speed_mode = FAN_PWM_SPEED,
        .channel = FAN_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = FAN_PWM_TIMER,
        .duty = fan.duty,
        .hpoint = fan.duty,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
    };
    esp_err_t ret;

    ret = ledc_timer_config(&pwmTimer);
    if (ret == ESP_OK)
    {
        ret = ledc_channel_config(&pwmChannel);
    }
    return ret;
}

esp_err_t InitializeNvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}

void app_main(void)
{
    const TickType_t delayTicks = 2000 / portTICK_PERIOD_MS;
    int16_t humidity;
    int16_t temperature;
    esp_err_t ret;

    ret = InitializePwmController();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "PWM channel set correctly");
    }

    ret = InitializeNvs();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "NVS initialized");
    }

    while(1)
    {
        ret = dht_read_data(DHT22_SENSOR_TYPE, DHT22_GPIO, &humidity, &temperature);
        
        if (humidity < fan.humidty_min)
        {
            fan.duty = 0;
        }
        else if (humidity > fan.humidity_max)
        {
            fan.duty = FAN_PWM_DUTY_MAX;
        }
        else
        {
            fan.duty = ((humidity - fan.humidty_min) * FAN_PWM_DUTY_MAX) / fan.steps;
        }

        ESP_ERROR_CHECK(ledc_set_duty(FAN_PWM_SPEED, FAN_PWM_CHANNEL, fan.duty));
        ESP_ERROR_CHECK(ledc_update_duty(FAN_PWM_SPEED, FAN_PWM_CHANNEL));
        if (ret == ESP_OK)
        {
            ESP_LOGI(LOG_TAG, "Temperature = %d, Humidity = %d", temperature, humidity);
        }
        vTaskDelay(delayTicks);
    }
}
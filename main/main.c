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
#define FAN_PWM_DUTY_MIN    (4 * FAN_PWM_DUTY_MAX / 10)
#define FAN_GPIO            GPIO_NUM_8

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

esp_err_t init_pwm_controller(void);
esp_err_t init_nvs(void);
void task_fan_control(void *);
void calculate_fan_duty(fan_control_state_t *fan, int16_t humidity);
void appy_fan_duty(fan_control_state_t *fan);

fan_control_state_t fan = {
    .humidty_min = 30 * 10,
    .humidity_max = 70 * 10,
    .steps = (70 - 30) * 10,
    .duty = 0,
};

esp_err_t init_pwm_controller(void)
{
    ledc_timer_config_t pwm_timer = {
        .speed_mode = FAN_PWM_SPEED,
        .duty_resolution = FAN_PWM_DUTY_RES,
        .timer_num = FAN_PWM_TIMER,
        .freq_hz = FAN_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ledc_channel_config_t pwm_channel = {
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

    ret = ledc_timer_config(&pwm_timer);
    if (ret == ESP_OK)
    {
        ret = ledc_channel_config(&pwm_channel);
    }
    return ret;
}

esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}

void calculate_fan_duty(fan_control_state_t *fan, int16_t humidity)
{
    if (humidity < fan->humidty_min)
    {
        fan->duty = FAN_PWM_DUTY_MIN;
    }
    else if (humidity > fan->humidity_max)
    {
        fan->duty = FAN_PWM_DUTY_MAX;
    }
    else
    {
        fan->duty = ((humidity - fan->humidty_min) * (FAN_PWM_DUTY_MAX - FAN_PWM_DUTY_MIN)) / fan->steps + FAN_PWM_DUTY_MIN;
    }
}

void appy_fan_duty(fan_control_state_t *fan)
{
    ESP_ERROR_CHECK(ledc_set_duty(FAN_PWM_SPEED, FAN_PWM_CHANNEL, fan->duty));
    ESP_ERROR_CHECK(ledc_update_duty(FAN_PWM_SPEED, FAN_PWM_CHANNEL));
}

void task_fan_control(void *)
{
    int16_t humidity;
    int16_t temperature;
    esp_err_t ret;
    const TickType_t delay_ticks = 2000 / portTICK_PERIOD_MS;

    while (1)
    {
        ret = dht_read_data(DHT22_SENSOR_TYPE, DHT22_GPIO, &humidity, &temperature);
        if (ret == ESP_OK)
        {
            calculate_fan_duty(&fan, humidity);
            appy_fan_duty(&fan);
            if (ret == ESP_OK)
            {
                ESP_LOGI(LOG_TAG, "Temperature = %d, Humidity = %d", temperature, humidity);
            }
        }
        else
        {
            fan.duty = FAN_PWM_DUTY_MIN;
            appy_fan_duty(&fan);
        }
        vTaskDelay(delay_ticks);
    }
}

void app_main(void)
{
    esp_err_t ret;

    ret = init_pwm_controller();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "PWM channel set correctly");
    }

    ret = init_nvs();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "NVS initialized");
    }

    TaskHandle_t fan_control_task_handle;

    xTaskCreate(task_fan_control,
                "fan-control",
                (configMINIMAL_STACK_SIZE * 8),
                NULL,
                (configMAX_PRIORITIES / 2),
                &fan_control_task_handle);

    vTaskSuspend(NULL);
}

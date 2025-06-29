#include "fan_controller.h"

#include <stdio.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

#include "dht.h"

#define LOG_TAG "FAN_CTRL_COMP"

static esp_err_t init_pwm_controller(void);
static void calculate_fan_duty(fan_control_state_t *fan, int16_t humidity);
static void apply_fan_duty(fan_control_state_t *fan);
static void task_fan_control(void *);

fan_control_state_t s_fan = {
    .humidty_min = 30 * 10,
    .humidity_max = 70 * 10,
    .steps = (70 - 30) * 10,
    .duty = 0,
    .control_mode = 0, // Default to automatic control
};
static SemaphoreHandle_t s_fan_control_mutex;

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
        .duty = s_fan.duty,
        .hpoint = s_fan.duty,
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

void apply_fan_duty(fan_control_state_t *fan)
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
            calculate_fan_duty(&s_fan, humidity);
            apply_fan_duty(&s_fan);
            if (ret == ESP_OK)
            {
                ESP_LOGI(LOG_TAG, "Temperature = %d, Humidity = %d", temperature, humidity);
            }
        }
        else
        {
            s_fan.duty = FAN_PWM_DUTY_MIN;
            apply_fan_duty(&s_fan);
        }
        vTaskDelay(delay_ticks);
    }
}

esp_err_t fan_controller_init(void)
{
    esp_err_t ret;

    // Create a mutex for protecting the fan_control_state_t structure
    s_fan_control_mutex = xSemaphoreCreateMutex();
    if (s_fan_control_mutex == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to create fan control mutex");
        return ESP_FAIL;
    }

    ret = init_pwm_controller();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "PWM channel set correctly");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to initialize PWM: %s", esp_err_to_name(ret));
        return ret;
    }

    TaskHandle_t fan_control_task_handle;
    xTaskCreate(task_fan_control,
                "fan-control",
                (configMINIMAL_STACK_SIZE * 8), // Increased stack size
                NULL,
                (configMAX_PRIORITIES / 2),
                &fan_control_task_handle);

    return ESP_OK;
}

fan_control_state_t* fan_controller_get_state_ptr(void)
{
    return NULL;
}

SemaphoreHandle_t fan_controller_get_mutex_handle(void)
{
    return NULL;
}

void fan_controller_calculate_duty(int16_t humidity)
{
    (void)humidity;
}


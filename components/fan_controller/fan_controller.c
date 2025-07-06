#include "fan_controller.h"

#include <stdio.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

#include "dht.h"

#define LOG_TAG "FAN_CTRL_COMP"

// PWM Defines (Public for components that need to interact with fan's PWM)
#define FAN_PWM_FREQ (25 * 1000)
#define FAN_PWM_TIMER LEDC_TIMER_0
#define FAN_PWM_SPEED LEDC_LOW_SPEED_MODE
#define FAN_PWM_CHANNEL LEDC_CHANNEL_0
#define FAN_PWM_DUTY_RES LEDC_TIMER_10_BIT
#define FAN_PWM_DUTY_MAX ((0x1u << ((size_t)FAN_PWM_DUTY_RES)) - 1)
#define FAN_PWM_DUTY_MIN (4 * FAN_PWM_DUTY_MAX / 10)
#define FAN_GPIO GPIO_NUM_8

#define DHT22_GPIO GPIO_NUM_10
#define DHT22_SENSOR_TYPE DHT_TYPE_AM2301

#define SEMAPHORE_TIMEOUT_MS 100
#define SEMAPHORE_TIMEOUT_TICKS pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)

static esp_err_t init_pwm_controller(void);
static void calculate_fan_duty(fan_control_state_t *fan, int16_t humidity);
static void apply_fan_duty(fan_control_state_t *fan);
static void task_fan_control(void *);

fan_control_state_t s_fan = {
    .humidity_min = 30 * 10,
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
    if (humidity < fan->humidity_min)
    {
        fan->duty = FAN_PWM_DUTY_MIN;
    }
    else if (humidity > fan->humidity_max)
    {
        fan->duty = FAN_PWM_DUTY_MAX;
    }
    else
    {
        fan->duty = ((humidity - fan->humidity_min) * (FAN_PWM_DUTY_MAX - FAN_PWM_DUTY_MIN)) / fan->steps + FAN_PWM_DUTY_MIN;
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

fan_control_state_t *fan_controller_get_state_ptr(void)
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

uint32_t fan_controller_get_max_duty(void)
{
    return FAN_PWM_DUTY_MAX;
}

uint32_t fan_controller_get_current_duty(void)
{
    return s_fan.duty;
}

int16_t fan_controller_get_min_humidity(void)
{
    return s_fan.humidity_min;
}

int16_t fan_controller_get_max_humidity(void)
{
    return s_fan.humidity_max;
}

fan_control_mode_t fan_controller_get_mode(void)
{
    return s_fan.control_mode;
}

esp_err_t fan_controller_set_current_duty(uint32_t duty)
{
    if (duty < FAN_PWM_DUTY_MIN || duty > FAN_PWM_DUTY_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        s_fan.control_mode = FAN_MODE_MANUAL;
        s_fan.duty = duty;
        xSemaphoreGive(s_fan_control_mutex);
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t fan_controller_set_min_humidty(int16_t humidity)
{
    if (humidity < 0 || humidity > 1000)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        s_fan.humidity_min = humidity;
        xSemaphoreGive(s_fan_control_mutex);
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t fan_controller_set_max_humidty(int16_t humidity)
{
    if (humidity < 0 || humidity > 1000)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        s_fan.humidity_max = humidity;
        xSemaphoreGive(s_fan_control_mutex);
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t fan_controller_set_mode(fan_control_mode_t mode)
{
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        s_fan.control_mode = mode;
        xSemaphoreGive(s_fan_control_mutex);
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_TIMEOUT;
    }
}

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

/**
 * @brief Initialize the PWM controller for fan speed control
 * 
 * Internal function that configures the ESP32's LEDC peripheral for PWM output.
 * Sets up a 25kHz PWM signal with 10-bit resolution on GPIO 8.
 * 
 * @return ESP_OK on successful PWM initialization
 * @return ESP error code on PWM configuration failure
 */
static esp_err_t init_pwm_controller(void);

/**
 * @brief Calculate fan duty cycle based on humidity
 * 
 * Internal function that implements the automatic control algorithm.
 * Maps humidity readings to fan speed using linear interpolation.
 * 
 * @param fan Pointer to fan control state structure
 * @param humidity Current humidity reading (0.1% units)
 */
static void calculate_fan_duty(fan_control_state_t *fan, int16_t humidity);

/**
 * @brief Apply the calculated duty cycle to the PWM output
 * 
 * Internal function that updates the PWM hardware with the new duty cycle.
 * 
 * @param fan Pointer to fan control state structure
 */
static void apply_fan_duty(fan_control_state_t *fan);

/**
 * @brief Background task for automatic fan control
 * 
 * Internal task that runs continuously to handle automatic humidity-based control.
 * Reads the DHT22 sensor every 2 seconds and adjusts fan speed in AUTO mode.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void task_fan_control(void *pvParameters);

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
    // Configure PWM timer for 25kHz frequency with 10-bit resolution
    ledc_timer_config_t pwm_timer = {
        .speed_mode = FAN_PWM_SPEED,
        .duty_resolution = FAN_PWM_DUTY_RES,
        .timer_num = FAN_PWM_TIMER,
        .freq_hz = FAN_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    
    // Configure PWM channel for fan control on GPIO 8
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

    // Initialize timer first, then channel
    ret = ledc_timer_config(&pwm_timer);
    if (ret == ESP_OK)
    {
        ret = ledc_channel_config(&pwm_channel);
    }
    return ret;
}

void calculate_fan_duty(fan_control_state_t *fan, int16_t humidity)
{
    // Thread-safe state update with mutex protection
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        if (humidity < fan->humidity_min)
        {
            // Below minimum threshold: run at minimum speed
            fan->duty = FAN_PWM_DUTY_MIN;
        }
        else if (humidity > fan->humidity_max)
        {
            // Above maximum threshold: run at maximum speed
            fan->duty = FAN_PWM_DUTY_MAX;
        }
        else
        {
            // Between thresholds: linear interpolation
            fan->duty = ((humidity - fan->humidity_min) * (FAN_PWM_DUTY_MAX - FAN_PWM_DUTY_MIN)) / fan->steps + FAN_PWM_DUTY_MIN;
        }
        xSemaphoreGive(s_fan_control_mutex);
    }
}

void apply_fan_duty(fan_control_state_t *fan)
{
    // Update PWM duty cycle and apply to hardware
    ESP_ERROR_CHECK(ledc_set_duty(FAN_PWM_SPEED, FAN_PWM_CHANNEL, fan->duty));
    ESP_ERROR_CHECK(ledc_update_duty(FAN_PWM_SPEED, FAN_PWM_CHANNEL));
}

void task_fan_control(void *pvParameters)
{
    int16_t humidity;
    int16_t temperature;
    esp_err_t ret;
    const TickType_t delay_ticks = 2000 / portTICK_PERIOD_MS; // 2 second delay

    while (1)
    {
        // Read humidity and temperature from DHT22 sensor
        ret = dht_read_data(DHT22_SENSOR_TYPE, DHT22_GPIO, &humidity, &temperature);
        
        if (s_fan.control_mode == FAN_MODE_AUTO) 
        {
            if (ret == ESP_OK)
            {
                // Successful sensor read: calculate and apply new duty cycle
                calculate_fan_duty(&s_fan, humidity);
                apply_fan_duty(&s_fan);
                ESP_LOGI(LOG_TAG, "Temperature = %d, Humidity = %d", temperature, humidity);
            }
            else
            {
                // Sensor read failed: fallback to minimum speed
                s_fan.duty = FAN_PWM_DUTY_MIN;
                apply_fan_duty(&s_fan);
                ESP_LOGW(LOG_TAG, "DHT sensor read failed, using minimum fan speed");
            }
        }
        // Else, fan duty must be set while mode is changed from AUTO to MANUAL
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
                (configMINIMAL_STACK_SIZE * 8), // Increased stack size for sensor operations
                NULL,
                (configMAX_PRIORITIES / 2),     // Medium priority
                &fan_control_task_handle);

    return ESP_OK;
}

fan_control_state_t *fan_controller_get_state_ptr(void)
{
    return &s_fan;
}

SemaphoreHandle_t fan_controller_get_mutex_handle(void)
{
    return s_fan_control_mutex;
}

uint32_t fan_controller_get_max_duty(void)
{
    return FAN_PWM_DUTY_MAX;
}

uint16_t fan_controller_get_current_duty(void)
{
    // Convert PWM duty cycle (0-1023) to percentage (0-1000)
    // 1023 -> 1000 (100%)
    // 511 -> 500 (50%)
    // 102 -> 100 (10%)
    return (1000 * s_fan.duty) / FAN_PWM_DUTY_MAX;
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
    // Validate percentage range (0-1000 for 0.0%-100.0%)
    if (duty > 1000)
    {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        s_fan.control_mode = FAN_MODE_MANUAL;
        // Convert percentage (0-1000) to PWM duty cycle (0-1023)
        // 1000 -> 1023 (100%)
        // 500 -> 511 (50%)
        // 100 -> 102 (10%)
        s_fan.duty = (duty * FAN_PWM_DUTY_MAX) / 1000;
        apply_fan_duty(&s_fan);
        xSemaphoreGive(s_fan_control_mutex);
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t fan_controller_set_min_humidity(int16_t humidity)
{
    if (humidity < 0 || humidity > 1000)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        s_fan.humidity_min = humidity;
        s_fan.steps = s_fan.humidity_max - s_fan.humidity_min;
        xSemaphoreGive(s_fan_control_mutex);
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t fan_controller_set_max_humidity(int16_t humidity)
{
    if (humidity < 0 || humidity > 1000)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_fan_control_mutex, SEMAPHORE_TIMEOUT_TICKS) == pdTRUE)
    {
        s_fan.humidity_max = humidity;
        s_fan.steps = s_fan.humidity_max - s_fan.humidity_min;
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

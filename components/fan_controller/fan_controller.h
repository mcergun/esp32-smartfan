#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include "esp_err.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
// No longer needs to include main.h for fan_control_state_t

// PWM Defines (Public for components that need to interact with fan's PWM)
#define FAN_PWM_FREQ        (25 * 1000)
#define FAN_PWM_TIMER       LEDC_TIMER_0
#define FAN_PWM_SPEED       LEDC_LOW_SPEED_MODE
#define FAN_PWM_CHANNEL     LEDC_CHANNEL_0
#define FAN_PWM_DUTY_RES    LEDC_TIMER_10_BIT
#define FAN_PWM_DUTY_MAX    ((0x1u << ((size_t)FAN_PWM_DUTY_RES)) - 1)
#define FAN_PWM_DUTY_MIN    (4 * FAN_PWM_DUTY_MAX / 10)
#define FAN_GPIO            GPIO_NUM_8

#define DHT22_GPIO          GPIO_NUM_10
#define DHT22_SENSOR_TYPE   DHT_TYPE_AM2301

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
    /// @brief current control mode (0: Auto, 1: Manual/BLE)
    uint8_t control_mode;
} fan_control_state_t;

/// @brief Initializes the fan controller, including PWM and creates the control task.
/// @return ESP_OK on success, otherwise an error code.
esp_err_t fan_controller_init(void);

/// @brief Gets a pointer to the global fan control state.
/// @param This pointer allows other components (like BLE) to read/write the state,
///        but direct access should be protected by the mutex.
/// @return Pointer to fan_control_state_t.
fan_control_state_t* fan_controller_get_state_ptr(void);

/// @brief Gets the handle to the mutex protecting the fan control state.
/// @return SemaphoreHandle_t for the fan control mutex.
SemaphoreHandle_t fan_controller_get_mutex_handle(void);

/// @brief Function to calculate fan duty (can be called by internal task or external source like BLE)
///        Note: This function now takes humidity and operates on the internal s_fan_state
/// @param humidity Current humidity level * 10
void fan_controller_calculate_duty(int16_t humidity);

#endif

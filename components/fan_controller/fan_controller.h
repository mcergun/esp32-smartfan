#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

typedef enum fan_control_mode
{
    FAN_MODE_AUTO,
    FAN_MODE_MANUAL,
} fan_control_mode_t;

typedef struct fan_control_state
{
    /// @brief minimum humidity value * 10 for fan to start working
    int16_t humidity_min;
    /// @brief maximum humidty value * 10 for max fan cycles
    int16_t humidity_max;
    /// @brief granularity of fan control steps
    int16_t steps;
    /// @brief current duty of fan
    uint32_t duty;
    /// @brief current control mode (0: Auto, 1: Manual/BLE)
    fan_control_mode_t control_mode;
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

uint32_t fan_controller_get_max_duty(void);
uint32_t fan_controller_get_current_duty(void);
int16_t fan_controller_get_min_humidty(void);
int16_t fan_controller_get_max_humidty(void);
fan_control_mode_t fan_controller_get_mode(void);

esp_err_t fan_controller_set_current_duty(uint32_t duty);
esp_err_t fan_controller_set_min_humidity(int16_t humidity);
esp_err_t fan_controller_set_max_humidity(int16_t humidity);
esp_err_t fan_controller_set_mode(fan_control_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif

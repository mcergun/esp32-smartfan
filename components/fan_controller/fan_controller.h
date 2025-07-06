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

uint32_t fan_controller_get_max_duty(void);

/**
 * @brief Get the current fan speed as a percentage
 * 
 * Returns the current fan speed as a percentage value.
 * This represents the current fan speed in 0.1% units (0-1000).
 * 
 * @return Current fan speed percentage (0-1000, representing 0.0%-100.0%)
 * 
 * @see fan_controller_set_current_duty()
 */
uint16_t fan_controller_get_current_duty(void);
int16_t fan_controller_get_min_humidity(void);
int16_t fan_controller_get_max_humidity(void);
fan_control_mode_t fan_controller_get_mode(void);

/**
 * @brief Set the fan speed as a percentage
 * 
 * Sets the fan speed as a percentage value, effectively controlling its speed.
 * This function automatically switches the controller to MANUAL mode.
 * 
 * @param duty Fan speed percentage (0-1000, representing 0.0%-100.0%)
 *             0 = fan off, 1000 = maximum speed
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if duty is out of valid range (0-1000)
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 * 
 * @note This function automatically switches to MANUAL mode
 * @note The duty cycle is applied immediately to the PWM output
 * @note Internal conversion handles PWM duty cycle mapping
 * 
 * @see fan_controller_get_current_duty()
 * @see fan_controller_set_mode()
 */
esp_err_t fan_controller_set_current_duty(uint32_t duty);
esp_err_t fan_controller_set_min_humidity(int16_t humidity);
esp_err_t fan_controller_set_max_humidity(int16_t humidity);
esp_err_t fan_controller_set_mode(fan_control_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif

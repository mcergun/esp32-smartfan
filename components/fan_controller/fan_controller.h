#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/**
 * @brief Fan control modes
 * 
 * Defines the available control modes for the fan controller.
 * The controller can operate in automatic mode (humidity-based) or
 * manual mode (externally controlled).
 */
typedef enum fan_control_mode
{
    FAN_MODE_AUTO,    /**< Automatic mode: Fan speed controlled by humidity sensor */
    FAN_MODE_MANUAL,  /**< Manual mode: Fan speed controlled by external commands */
} fan_control_mode_t;

/**
 * @brief Fan control state structure
 * 
 * Contains all the state information for the fan controller.
 * This structure is protected by a mutex and should not be accessed
 * directly without proper synchronization.
 */
typedef struct fan_control_state
{
    int16_t humidity_min;      /**< Minimum humidity threshold (0.1% units, e.g., 300 = 30.0%) */
    int16_t humidity_max;      /**< Maximum humidity threshold (0.1% units, e.g., 700 = 70.0%) */
    int16_t steps;             /**< Granularity of fan control steps (humidity_max - humidity_min) */
    uint32_t duty;             /**< Current PWM duty cycle (0-1023 for 10-bit resolution) */
    fan_control_mode_t control_mode; /**< Current control mode (AUTO or MANUAL) */
} fan_control_state_t;

/**
 * @brief Initialize the fan controller
 * 
 * This function initializes the fan controller system, including:
 * - PWM configuration for fan control
 * - DHT22 sensor initialization
 * - Creation of the fan control task
 * - Initialization of thread-safe state management
 * 
 * @note This function must be called before any other fan controller functions.
 * @note The fan control task runs continuously and monitors humidity in AUTO mode.
 * 
 * @return ESP_OK on successful initialization
 * @return ESP_FAIL if mutex creation fails
 * @return Other ESP error codes for PWM initialization failures
 * 
 * @see fan_controller_set_mode()
 * @see fan_controller_set_current_duty()
 */
esp_err_t fan_controller_init(void);

/**
 * @brief Get pointer to the global fan control state
 * 
 * Returns a pointer to the internal fan control state structure.
 * This allows other components (like BLE controllers) to access the state,
 * but direct access should be protected by the mutex.
 * 
 * @warning Direct modification of the state structure is not thread-safe.
 * Use the provided setter functions instead, or protect access with the mutex.
 * 
 * @return Pointer to the fan_control_state_t structure
 * 
 * @see fan_controller_get_mutex_handle()
 * @see fan_controller_set_current_duty()
 * @see fan_controller_set_mode()
 */
fan_control_state_t* fan_controller_get_state_ptr(void);

/**
 * @brief Get the mutex handle for thread-safe state access
 * 
 * Returns the handle to the mutex that protects the fan control state.
 * This should be used when direct access to the state structure is required.
 * 
 * @return SemaphoreHandle_t for the fan control mutex
 * 
 * @see fan_controller_get_state_ptr()
 */
SemaphoreHandle_t fan_controller_get_mutex_handle(void);

/**
 * @brief Get the maximum PWM duty cycle value
 * 
 * Returns the maximum duty cycle value that can be set for the fan.
 * This is determined by the PWM resolution (10-bit = 1023).
 * 
 * @return Maximum duty cycle value (1023 for 10-bit PWM)
 */
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

/**
 * @brief Get the minimum humidity threshold
 * 
 * Returns the minimum humidity threshold in 0.1% units.
 * For example, a return value of 300 represents 30.0%.
 * 
 * @return Minimum humidity threshold (0.1% units)
 * 
 * @see fan_controller_set_min_humidity()
 */
int16_t fan_controller_get_min_humidity(void);

/**
 * @brief Get the maximum humidity threshold
 * 
 * Returns the maximum humidity threshold in 0.1% units.
 * For example, a return value of 700 represents 70.0%.
 * 
 * @return Maximum humidity threshold (0.1% units)
 * 
 * @see fan_controller_set_max_humidity()
 */
int16_t fan_controller_get_max_humidity(void);

/**
 * @brief Get the current control mode
 * 
 * Returns the current fan control mode (AUTO or MANUAL).
 * 
 * @return Current control mode
 * 
 * @see fan_controller_set_mode()
 * @see fan_control_mode_t
 */
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

/**
 * @brief Set the minimum humidity threshold
 * 
 * Sets the minimum humidity threshold for automatic fan control.
 * When humidity is below this threshold, the fan runs at minimum speed.
 * 
 * @param humidity Minimum humidity threshold (0-1000, representing 0.0%-100.0%)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if humidity is out of valid range (0-1000)
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 * 
 * @note The humidity value is stored in 0.1% units internally
 * @note This affects the automatic control algorithm
 * @note The step granularity is recalculated when this value changes
 * 
 * @see fan_controller_get_min_humidity()
 * @see fan_controller_set_max_humidity()
 */
esp_err_t fan_controller_set_min_humidity(int16_t humidity);

/**
 * @brief Set the maximum humidity threshold
 * 
 * Sets the maximum humidity threshold for automatic fan control.
 * When humidity is above this threshold, the fan runs at maximum speed.
 * 
 * @param humidity Maximum humidity threshold (0-1000, representing 0.0%-100.0%)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if humidity is out of valid range (0-1000)
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 * 
 * @note The humidity value is stored in 0.1% units internally
 * @note This affects the automatic control algorithm
 * @note The step granularity is recalculated when this value changes
 * 
 * @see fan_controller_get_max_humidity()
 * @see fan_controller_set_min_humidity()
 */
esp_err_t fan_controller_set_max_humidity(int16_t humidity);

/**
 * @brief Set the fan control mode
 * 
 * Switches between automatic and manual control modes.
 * In AUTO mode, the fan speed is controlled by humidity sensor readings.
 * In MANUAL mode, the fan speed is controlled by external commands.
 * 
 * @param mode Control mode to set (FAN_MODE_AUTO or FAN_MODE_MANUAL)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 * 
 * @note Switching to AUTO mode will recalculate fan speed based on current humidity
 * @note Switching to MANUAL mode preserves the current fan speed
 * @note The background task only processes humidity readings in AUTO mode
 * 
 * @see fan_control_mode_t
 * @see fan_controller_get_mode()
 */
esp_err_t fan_controller_set_mode(fan_control_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* FAN_CONTROLLER_H */

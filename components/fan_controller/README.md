# Fan Controller Component

This component provides a thread-safe, flexible fan control system for ESP32-based projects. It supports both automatic (humidity-based) and manual fan speed control, using PWM for speed regulation and a DHT22 sensor for environmental feedback.

## Features

- **Automatic Mode:** Fan speed is adjusted based on humidity readings from a DHT22 sensor.
- **Manual Mode:** Fan speed can be set directly by external commands (e.g., via BLE or UI).
- **Thread-Safe State Management:** All state changes are protected by a mutex for safe multi-tasking.
- **PWM Control:** Uses ESP32's LEDC peripheral for smooth, high-frequency fan speed control.
- **Configurable Humidity Thresholds:** Set minimum and maximum humidity for automatic control.
- **Sensor Data Access:** Provides current humidity and temperature readings.

---

## API Overview

### Initialization

```c
esp_err_t fan_controller_init(void);
```
Initializes the fan controller, PWM, DHT22 sensor, mutex, and starts the control task.  
**Must be called before any other API function.**

---

### State Access

```c
fan_control_state_t* fan_controller_get_state_ptr(void);
SemaphoreHandle_t fan_controller_get_mutex_handle(void);
```
- Get a pointer to the internal state (use mutex for thread safety).
- Get the mutex handle for direct state access.

---

### Fan Speed Control

```c
uint32_t fan_controller_get_max_duty(void);
uint16_t fan_controller_get_current_duty(void);
esp_err_t fan_controller_set_current_duty(uint32_t duty);
```
- Get the maximum duty cycle (PWM).
- Get/set the current fan speed as a percentage (0-1000 = 0.0%-100.0%).
- Setting the duty switches to MANUAL mode.

---

### Humidity Thresholds

```c
int16_t fan_controller_get_min_humidity(void);
int16_t fan_controller_get_max_humidity(void);
esp_err_t fan_controller_set_min_humidity(int16_t humidity);
esp_err_t fan_controller_set_max_humidity(int16_t humidity);
```
- Get/set minimum and maximum humidity thresholds (0-1000 = 0.0%-100.0%).
- Used for automatic mode control.

---

### Control Mode

```c
fan_control_mode_t fan_controller_get_mode(void);
esp_err_t fan_controller_set_mode(fan_control_mode_t mode);
```
- Get/set the current control mode (`FAN_MODE_AUTO` or `FAN_MODE_MANUAL`).

---

### Sensor Data

```c
int16_t fan_controller_get_current_humidity(void);
int16_t fan_controller_get_current_temperature(void);
```
- Get the latest humidity and temperature readings (0.1% and 0.1°C units).

---

## Usage Example

### 1. Initialization

```c
#include "fan_controller.h"

void app_main(void) {
    if (fan_controller_init() != ESP_OK) {
        // Handle error
    }
}
```

### 2. Set Manual Fan Speed

```c
// Set fan speed to 50% (500/1000)
fan_controller_set_current_duty(500);
```

### 3. Switch to Automatic Mode

```c
fan_controller_set_mode(FAN_MODE_AUTO);
```

### 4. Adjust Humidity Thresholds

```c
fan_controller_set_min_humidity(300); // 30.0%
fan_controller_set_max_humidity(700); // 70.0%
```

### 5. Read Sensor Data

```c
int16_t humidity = fan_controller_get_current_humidity(); // e.g., 450 = 45.0%
int16_t temperature = fan_controller_get_current_temperature(); // e.g., 250 = 25.0°C
```

### 6. Thread-Safe State Access

```c
SemaphoreHandle_t mutex = fan_controller_get_mutex_handle();
fan_control_state_t* state = fan_controller_get_state_ptr();

if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Safe to read/modify state
    int16_t min_humidity = state->humidity_min;
    xSemaphoreGive(mutex);
}
```

---

## Notes

- The fan control task runs in the background and updates sensor readings every 2 seconds.
- In AUTO mode, the fan speed is automatically adjusted based on humidity.
- In MANUAL mode, the fan speed remains fixed until changed.
- All setters are thread-safe; direct state access requires mutex protection.

---

## Integration

- Add `fan_controller.c` and `fan_controller.h` to your project.
- Ensure the DHT22 sensor is connected to the correct GPIO (default: GPIO 10).
- The fan PWM output is on GPIO 8 by default.
- Requires FreeRTOS, ESP-IDF LEDC, and DHT driver.

---

## License

See the main project license.

---

## Authors

See project contributors.

---

This README should help you understand, use, and integrate the `fan_controller` component in your ESP32 project. If you need more advanced usage or encounter issues, refer to the code comments and function documentation. 
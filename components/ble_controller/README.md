# BLE Controller Component

This component provides a Bluetooth Low Energy (BLE) interface for remotely controlling and monitoring the smart fan system. It integrates with the fan controller to allow wireless configuration of fan speed, mode, and automatic control thresholds, as well as real-time status and sensor data reporting.

## Features

- **Remote Fan Control:** Set fan speed and mode via BLE commands
- **Automatic Mode Configuration:** Adjust humidity thresholds for automatic fan control
- **Real-Time Status Monitoring:** Query current fan state, speed, mode, and sensor readings
- **Thread-Safe Operations:** All state changes and status updates are protected by mutexes
- **Comprehensive Error Handling:** Input validation and detailed status feedback
- **BLE GATT Service:** Exposes read/write characteristics for command and status exchange

---

## BLE Command Set

### Fan Control Commands

| Command           | Description                                 | Example         | Response                        |
|-------------------|---------------------------------------------|-----------------|----------------------------------|
| `SET AUTO`        | Switch to automatic humidity-based control  | `SET AUTO`      | "Fan mode set to AUTO"          |
| `SET MANUAL`      | Switch to manual control mode               | `SET MANUAL`    | "Fan mode set to MANUAL"        |
| `SET <speed>`     | Set fan speed (0-100 for 0-100%)           | `SET 50`        | "Fan speed set to 50"           |
| `SET MIN <val>`   | Set min humidity threshold (0-100%)         | `SET MIN 30`    | "Min humidity set to 30.0%"     |
| `SET MAX <val>`   | Set max humidity threshold (0-100%)         | `SET MAX 70`    | "Max humidity set to 70.0%"     |
| `GET STATUS`      | Request current fan status                  | `GET STATUS`    | Status response via read         |

### Command Format
- **Speed Control:** `SET <value>` where value is 0-100 (percent)
- **Humidity Thresholds:** `SET MIN/MAX <value>` where value is 0-100 (percent)
- **Mode Control:** `SET AUTO` or `SET MANUAL`
- **Status Request:** `GET STATUS`

---

## Status Response Format

When reading from the BLE status characteristic, the response includes:

```
Fan: ON/OFF, Speed: X.X%, Mode: AUTO/MANUAL, Temperature: X.XC, Humidity: X.X%, Min: XX%, Max: XX%, Status: <last_command_result>
```

**Example:**
```
Fan: ON, Speed: 40.0%, Mode: AUTO, Temperature: 25.0C, Humidity: 50.0%, Min: 20%, Max: 80%, Status: Min humidity set to 20.0%
```

---

## Usage Examples

### 1. Set Manual Fan Speed
```
SET MANUAL
SET 60      # Set fan to 60%
GET STATUS  # Check status
```

### 2. Switch to Automatic Mode and Configure
```
SET AUTO
SET MIN 30  # Set min humidity to 30%
SET MAX 70  # Set max humidity to 70%
GET STATUS
```

### 3. Query Status
```
GET STATUS
```

---

## Integration

- Add `ble_controller.c` and `ble_controller.h` to your project.
- Ensure the fan controller is initialized before the BLE controller.
- BLE controller exposes a GATT service with:
  - **Read Characteristic:** Status and sensor data
  - **Write Characteristic:** Command interface
- Device advertises as `SmartFan-BLE` by default.

---

## Implementation Details

- **Thread Safety:** All fan control and status operations are mutex-protected.
- **Error Handling:** Invalid commands and out-of-range values return error status messages.
- **Command Processing:** Specific commands (`SET MIN`, `SET MAX`) are matched before generic ones (`SET`).
- **BLE GATT:**
  - Primary Service UUID: `0x180`
  - Read Characteristic UUID: `0xFEF4`
  - Write Characteristic UUID: `0xDEAD`

---

## Troubleshooting

- **Command Not Recognized:** Check command format and spelling.
- **Invalid Parameter:** Ensure values are within valid ranges (0-100).
- **No Response:** Check BLE connection and characteristic access.
- **Fan Not Responding:** Verify fan controller and BLE controller initialization order.
- **Debug Logging:** Enable with `esp_log_level_set("BLE-Server", ESP_LOG_DEBUG);`

---

## Authors

See project contributors.

---

This README should help you understand, use, and integrate the BLE controller component for wireless fan control. For advanced usage or troubleshooting, refer to the code comments and function documentation. 
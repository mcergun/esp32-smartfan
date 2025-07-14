#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"
#include "fan_controller.h"

char *TAG = "BLE-Server";
uint8_t ble_addr_type;
void ble_app_advertise(void);

// Global variable to store the last status message for BLE read operations
static char status_message[256];
static SemaphoreHandle_t status_mutex;

// Helper function to update status message
static void update_status_message(const char* format, ...)
{
    if (xSemaphoreTake(status_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(status_message, sizeof(status_message), format, args);
        va_end(args);
        xSemaphoreGive(status_mutex);
    }
}

// Process fan control commands
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *data = (char *)ctxt->om->om_data;
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Received command: %.*s", ctxt->om->om_len, data);
    
    // Fan control commands
    const char *cmd_auto = "SET AUTO";
    const char *cmd_manual = "SET MANUAL";
    size_t len_auto = strlen(cmd_auto);
    size_t len_manual = strlen(cmd_manual);

    if ((ctxt->om->om_len >= len_auto && memcmp(data, cmd_auto, len_auto) == 0 &&
         (ctxt->om->om_len == len_auto || data[len_auto] == '\0' || data[len_auto] == ' ')))
    {
        ret = fan_controller_set_mode(FAN_MODE_AUTO);
        if (ret == ESP_OK) {
            update_status_message("Fan mode set to AUTO");
            ESP_LOGI(TAG, "Fan mode set to AUTO");
        } else {
            update_status_message("Failed to set fan mode to AUTO");
            ESP_LOGE(TAG, "Failed to set fan mode to AUTO: %s", esp_err_to_name(ret));
        }
    }
    else if ((ctxt->om->om_len >= len_manual && memcmp(data, cmd_manual, len_manual) == 0 &&
              (ctxt->om->om_len == len_manual || data[len_manual] == '\0' || data[len_manual] == ' ')))
    {
        ret = fan_controller_set_mode(FAN_MODE_MANUAL);
        if (ret == ESP_OK) {
            update_status_message("Fan mode set to MANUAL");
            ESP_LOGI(TAG, "Fan mode set to MANUAL");
        } else {
            update_status_message("Failed to set fan mode to MANUAL");
            ESP_LOGE(TAG, "Failed to set fan mode to MANUAL: %s", esp_err_to_name(ret));
        }
    }
    else if (ctxt->om->om_len >= 8 && memcmp(data, "SET MIN ", 8) == 0) {
        char cmd[32];
        size_t len = ctxt->om->om_len < sizeof(cmd)-1 ? ctxt->om->om_len : sizeof(cmd)-1;
        memcpy(cmd, data, len);
        cmd[len] = '\0';
        int humidity = atoi(cmd + 8);
        if (humidity >= 0 && humidity <= 100) {
            int humidity_internal = humidity * 10;
            ret = fan_controller_set_min_humidity(humidity_internal);
            if (ret == ESP_OK) {
                update_status_message("Min humidity set to %d.0%%", humidity);
                ESP_LOGI(TAG, "Min humidity set to %d.0%%", humidity);
            } else {
                update_status_message("Failed to set min humidity");
                ESP_LOGE(TAG, "Failed to set min humidity: %s", esp_err_to_name(ret));
            }
        } else {
            update_status_message("Invalid humidity value (0-100)");
            ESP_LOGE(TAG, "Invalid humidity value: %d", humidity);
        }
    }
    else if (ctxt->om->om_len >= 8 && memcmp(data, "SET MAX ", 8) == 0) {
        char cmd[32];
        size_t len = ctxt->om->om_len < sizeof(cmd)-1 ? ctxt->om->om_len : sizeof(cmd)-1;
        memcpy(cmd, data, len);
        cmd[len] = '\0';
        int humidity = atoi(cmd + 8);
        if (humidity >= 0 && humidity <= 100) {
            int humidity_internal = humidity * 10;
            ret = fan_controller_set_max_humidity(humidity_internal);
            if (ret == ESP_OK) {
                update_status_message("Max humidity set to %d.0%%", humidity);
                ESP_LOGI(TAG, "Max humidity set to %d.0%%", humidity);
            } else {
                update_status_message("Failed to set max humidity");
                ESP_LOGE(TAG, "Failed to set max humidity: %s", esp_err_to_name(ret));
            }
        } else {
            update_status_message("Invalid humidity value (0-100)");
            ESP_LOGE(TAG, "Invalid humidity value: %d", humidity);
        }
    }
    else if (ctxt->om->om_len >= 4 && memcmp(data, "SET ", 4) == 0) {
        // Copy and null-terminate for further parsing
        char cmd[32];
        size_t len = ctxt->om->om_len < sizeof(cmd)-1 ? ctxt->om->om_len : sizeof(cmd)-1;
        memcpy(cmd, data, len);
        cmd[len] = '\0';
        // Now parse cmd+4 for the value
        int speed_percent = atoi(cmd + 4);
        if (speed_percent >= 0 && speed_percent <= 100) {
            ret = fan_controller_set_current_duty(speed_percent * 10);
            if (ret == ESP_OK) {
                update_status_message("Fan speed set to %d", speed_percent);
                ESP_LOGI(TAG, "Fan speed set to %d", speed_percent);
            } else {
                update_status_message("Failed to set fan speed");
                ESP_LOGE(TAG, "Failed to set fan speed: %s", esp_err_to_name(ret));
            }
        } else {
            update_status_message("Invalid speed value (0-1000)");
            ESP_LOGE(TAG, "Invalid speed value: %d", speed_percent);
        }
    }
    else if (ctxt->om->om_len >= 10 && memcmp(data, "GET STATUS", 10) == 0 &&
             (ctxt->om->om_len == 10 || data[10] == '\0' || data[10] == ' '))
    {
        // Status will be returned in the read operation
        ESP_LOGI(TAG, "Fan status requested");
    }
    else
    {
        update_status_message("Unknown command: %.*s", ctxt->om->om_len, data);
        ESP_LOGW(TAG, "Unknown command: %.*s", ctxt->om->om_len, data);
    }
    
    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char response[512];
    
    // Get current fan status
    uint16_t current_duty = fan_controller_get_current_duty();
    fan_control_mode_t mode = fan_controller_get_mode();
    int16_t current_temperature = fan_controller_get_current_temperature();
    int16_t current_humidity = fan_controller_get_current_humidity();
    
    // Format status response
    snprintf(response, sizeof(response), 
             "Fan: %s, Speed: %d.%d%%, Mode: %s, Temperature: %d.%dC, Humidity: %d.%d%%, Status: %s",
             current_duty > 0 ? "ON" : "OFF",
             current_duty / 10, current_duty % 10,
             mode == FAN_MODE_AUTO ? "AUTO" : "MANUAL",
             current_temperature / 10, current_temperature % 10,
             current_humidity / 10, current_humidity % 10,
             status_message);
    
    os_mbuf_append(ctxt->om, response, strlen(response));
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x180),                 // Define UUID for device type
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0xFEF4),           // Define UUID for reading
          .flags = BLE_GATT_CHR_F_READ,
          .access_cb = device_read},
         {.uuid = BLE_UUID16_DECLARE(0xDEAD),           // Define UUID for writing
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write},
         {0}}},
    {0}};

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECTED");
        ble_app_advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;
    default:
        break;
    }
    return 0;
}

// Define the BLE connection
void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ble_app_advertise();                     // Define the BLE connection
}

// The infinite task
void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

esp_err_t ble_control_init(void)
{
    // Initialize status mutex
    status_mutex = xSemaphoreCreateMutex();
    if (status_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create status mutex");
        return ESP_FAIL;
    }
    
    // Initialize status message
    strcpy(status_message, "BLE Controller initialized");
    
    // esp_nimble_hci_and_controller_init();      // 2 - Initialize ESP controller
    nimble_port_init();                        // 3 - Initialize the host stack
    ble_svc_gap_device_name_set("SmartFan-BLE"); // 4 - Initialize NimBLE configuration - server name
    ble_svc_gap_init();                        // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                       // 4 - Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);            // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);             // 4 - Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;      // 5 - Initialize application
    nimble_port_freertos_init(host_task);      // 6 - Run the thread
    return ESP_OK;
}

void ble_host_task(void *param)
{
    host_task(param);
}
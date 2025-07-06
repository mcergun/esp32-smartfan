#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

#include "dht.h"
#include "fan_controller.h"
#include "ble_controller.h"

#define LOG_TAG "FANCTL"

#define BLINK_GPIO          GPIO_NUM_8
#define DHT22_GPIO          GPIO_NUM_10
#define DHT22_SENSOR_TYPE   DHT_TYPE_AM2301

esp_err_t init_nvs(void);

esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}

void app_main(void)
{
    esp_err_t ret;

    ret = init_nvs();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "NVS initialized");
    }

    ret = fan_controller_init();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "Fan controller initialized");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Fan controller initialization failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = ble_control_init();
    if (ret == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "BLE fan service initialized");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "BLE controller initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(LOG_TAG, "SmartFan system initialized successfully");
    ESP_LOGI(LOG_TAG, "Connect to BLE device 'SmartFan-BLE' to control the fan");
    
    vTaskSuspend(NULL);
}

#include "esp_bt.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gap.h"
#include "esp_nimble_hci.h"
#include "esp_log.h"

// Example advertising parameters
static uint8_t ble_addr_type;
static const char *TAG = "BLE_FAN";

void ble_app_advertise(void);

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected");
        } else {
            ESP_LOGI(TAG, "Connect failed; retrying");
            ble_app_advertise();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected; restarting advertise");
        ble_app_advertise();
        break;
    default:
        break;
    }
    return 0;
}

void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields;

    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    const char *name = "FAN_CTRL";
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE("BLE_ADVERTISE", "Error setting adv fields: %d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable undirected
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // general discoverable

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE("BLE_ADVERTISE", "Error starting adv: %d", rc);
        return;
    }

    ESP_LOGI("BLE_ADVERTISE", "Advertising started");
}

void ble_control_host_task(void *param)
{
    nimble_port_run();
}

static void ble_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_advertise();
}

esp_err_t ble_control_init(void)
{
    esp_err_t ret;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE("BLE_INIT", "bt_controller_init failed: %d", ret);
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE("BLE_INIT", "bt_controller_enable failed: %d", ret);
        return ret;
    }

    ret = esp_nimble_hci_init();
    if (ret) {
        ESP_LOGE("BLE_INIT", "esp_nimble_hci_init failed: %d", ret);
        return ret;
    }

    ret = nimble_port_init();
    if (ret) {
        ESP_LOGE("BLE_INIT", "nimble_port_init failed: %d", ret);
        return ret;
    }

    ble_hs_cfg.sync_cb = ble_on_sync;

    nimble_port_freertos_init(ble_control_host_task);

    return ESP_OK;
}

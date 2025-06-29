#ifndef _BLE_CONTROLLER_H_
#define _BLE_CONTROLLER_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ble_control_init(void);

void ble_host_task(void *param);

#ifdef __cplusplus
}
#endif

#endif

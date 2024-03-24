//
// Created by yang on 2023/7/28.
//

#ifndef EPD_ANIYA_BOX_BLE_SERVER_H
#define EPD_ANIYA_BOX_BLE_SERVER_H

#include "esp_types.h"
#include "esp_event.h"

#define PREFERRED_MTU_VALUE       CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU

ESP_EVENT_DECLARE_BASE(BIKE_BLE_SERVER_EVENT);

typedef enum {
    BLE_SERVER_EVENT_START_ADV = 0,
    BLE_SERVER_EVENT_CONNECTED,
    BLE_SERVER_EVENT_DISCONNECTED,
    BLE_SERVER_EVENT_READ_WRITE,
} ble_server_event_id_t;

#include <esp_err.h>
#include "common_utils.h"

esp_err_t ble_server_init();

esp_err_t ble_server_start_adv(uint16_t duration);

esp_err_t ble_server_stop_adv();

esp_err_t ble_server_deinit();

#endif //EPD_ANIYA_BOX_BLE_SERVER_H

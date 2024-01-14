//
// Created by yang on 2023/7/28.
//

#ifndef EPD_ANIYA_BOX_BLE_SERVER_H
#define EPD_ANIYA_BOX_BLE_SERVER_H

#define PREFERRED_MTU_VALUE       512

#include <esp_err.h>
#include "common_utils.h"

esp_err_t ble_server_init();

esp_err_t ble_server_start_adv(uint16_t duration);

bool ble_server_is_adv();

esp_err_t ble_server_stop_adv();

esp_err_t ble_server_deinit();

#endif //EPD_ANIYA_BOX_BLE_SERVER_H

//
// Created by yang on 2023/12/24.
//

#ifndef HELLO_WORLD_WIFI_DEVICE_SETTING_H
#define HELLO_WORLD_WIFI_DEVICE_SETTING_H

#include "esp_http_server.h"
#include "my_file_server_common.h"

esp_err_t device_setting_reg_uri(httpd_handle_t server, http_server_t *my_http_server);

esp_err_t device_setting_get_handler(httpd_req_t *req);

esp_err_t device_setting_post_handler(httpd_req_t *req);

esp_err_t device_setting_unreg_uri(httpd_handle_t server);

#endif //HELLO_WORLD_WIFI_DEVICE_SETTING_H

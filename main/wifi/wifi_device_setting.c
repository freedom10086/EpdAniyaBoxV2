//
// Created by yang on 2023/12/24.
//

#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "wifi_device_setting.h"
#include "rx8025t.h"

#define MAX_BUFF_SIZE 256

#define TAG "device_setting_handler"

static uint8_t year, month, day, week, hour, minute, second;

esp_err_t device_setting_reg_uri(httpd_handle_t server, http_server_t *my_http_server) {
    httpd_uri_t setting = {
            .uri       = "/setting",
            .method    = HTTP_GET,
            .handler   = device_setting_get_handler,
            .user_ctx  = my_http_server
    };
    httpd_register_uri_handler(server, &setting);

    httpd_uri_t post_setting = {
            .uri       = "/setting",
            .method    = HTTP_POST,
            .handler   = device_setting_post_handler,
            .user_ctx  = my_http_server
    };

    ESP_LOGI(TAG, "Device Setting register successful!");

    return httpd_register_uri_handler(server, &post_setting);
}

esp_err_t device_setting_get_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");        //跨域传输协议

    static char json_response[1024];
    char *p = json_response;

    wifi_ps_type_t wifi_ps_type;
    esp_wifi_get_ps(&wifi_ps_type);

    rx8025t_get_time(&year, &month, &day, &week, &hour, &minute, &second);

    *p++ = '{';
    p += sprintf(p, "\"date_time\":\"20%02d-%02d-%02d %02d:%02d:%02d %d\",", year, month, day, week, hour, minute, second);
    *p++ = '}';
    *p++ = 0;

    httpd_resp_set_type(req, "application/json");       // 设置http响应类型
    return httpd_resp_send(req, json_response, strlen(json_response));
}

esp_err_t device_setting_post_handler(httpd_req_t *req) {
    int remaining = req->content_len;
    if (remaining > MAX_BUFF_SIZE || remaining <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Device Setting Post uri : %s", req->uri);

    int data_read;
    char *read_buf = malloc(sizeof(char) * remaining);

    ESP_LOGI(TAG, "Post Content size : %d", remaining);
    data_read = httpd_req_recv(req, read_buf, remaining);

    if (data_read != remaining) {
        free(read_buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
        return ESP_FAIL;;
    }

    // parse
    //
    return ESP_OK;
}

esp_err_t device_setting_unreg_uri(httpd_handle_t server) {
    return httpd_unregister_uri(server, "/setting");
}
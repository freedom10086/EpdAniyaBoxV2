#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include "wifi/wifi_ap.h"

#include "image_manage_page.h"
#include "view/list_view.h"

#include "lcd/display.h"
#include "page_manager.h"
#include "battery.h"
#include "qrcode.h"

#define TAG "setting-page"
#define HTTP_PREFIX "http://"


static bool wifi_on = false;

static char draw_buff[64] = {};

void image_manage_page_on_create(void *arg) {
    //ESP_ERR_WIFI_NOT_INIT
    wifi_mode_t wifi_mode;
    esp_err_t err = esp_wifi_get_mode(&wifi_mode);
    ESP_LOGI(TAG, "current wifi mdoe: %d, %d %s", wifi_mode, err, esp_err_to_name(err));
    if (ESP_ERR_WIFI_NOT_INIT == err) {
        wifi_on = false;
    } else {
        wifi_on = wifi_mode != WIFI_MODE_NULL;
    }
    ESP_LOGI(TAG, "current wifi status: %s", wifi_on ? "on" : "off");

    if (!wifi_on) {
        wifi_init_softap();
        wifi_on = true;
    }
}

void image_manage_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    int y = 8;
    epd_paint_draw_string_at(epd_paint, 2, y,
                             (char[]) {0xC1, 0xAC, 0xBD, 0xD3, 0x57, 0x49, 0x46, 0x49, 0xB9, 0xDC,
                                       0xC0, 0xED, 0xCD, 0xBC, 0xC6, 0xAC,
                                       0x3A, 0x00}, &Font_HZK16, 1);
    y += 20;

    sprintf(draw_buff, "SSID:%s", CONFIG_WIFI_SSID);
    epd_paint_draw_string_at(epd_paint, 4, y, draw_buff, &Font16, 1);
    y += 20;

    // 密码:
    epd_paint_draw_string_at(epd_paint, 4, y, (char[]) {0xC3, 0xDC, 0xC2, 0xEB, 0x3A, 0x00}, &Font_HZK16, 1);
    epd_paint_draw_string_at(epd_paint, 52, y, CONFIG_WIFI_PASSWORD, &Font_HZK16, 1);
    y += 20;

    esp_netif_ip_info_t ip_info;
    esp_err_t err = wifi_ap_get_ip(&ip_info);
    if (ESP_OK == err) {
        memcpy(draw_buff, HTTP_PREFIX, strlen(HTTP_PREFIX));
        esp_ip4addr_ntoa((const esp_ip4_addr_t *) &ip_info.ip.addr,
                         draw_buff + strlen(HTTP_PREFIX),
                         sizeof(draw_buff) - strlen(HTTP_PREFIX));
        epd_paint_draw_string_at(epd_paint, 4, y, draw_buff, &Font16, 1);

        y += 20;

        esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();

        sprintf(draw_buff, "WIFI:T:WPA;S:%s;P:%s;", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
        esp_qrcode_generate(&cfg, (const char *) draw_buff);
    } else {
        epd_paint_draw_string_at(epd_paint, 4, y,
                                 (char[]) {0xBB, 0xF1, 0xC8, 0xA1, 0x49, 0x50,
                                           0xB5, 0xD8, 0xD6, 0xB7, 0xD6, 0xD0, 0x2E,
                                           0x00}, &Font_HZK16, 1);
    }
    y += 20;

    epd_paint_draw_string_at(epd_paint, 52, 174,
                             (char[]) {0xB0, 0xB4, 0xC8, 0xCE, 0xD2, 0xE2, 0xBC,
                                       0xFC, 0xCD, 0xCB, 0xB3, 0xF6, 0x00},
                             &Font_HZK16, 1);

}

bool image_manage_page_key_click(key_event_id_t key_event_type) {
    if (KEY_1_SHORT_CLICK == key_event_type || KEY_2_SHORT_CLICK == key_event_type) {
        page_manager_close_page();
        page_manager_request_update(false);
        return true;
    }
    return false;
}

void image_manage_page_on_destroy(void *arg) {
    if (wifi_on) {
        wifi_deinit_softap();
        wifi_on = false;
    }
}

int image_manage_page_on_enter_sleep(void *args) {
    if (!battery_is_curving()) {
        return -1;
    }
    return -1;
}
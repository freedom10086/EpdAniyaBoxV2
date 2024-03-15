#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <esp_log.h>

#include "common_utils.h"
#include "lcd/epdpaint.h"
#include "view/digi_view.h"
#include "view/battery_view.h"

#include "temperature_page.h"
#include "sht40.h"
#include "battery.h"
#include "static/static.h"
#include "page_manager.h"

#include "esp_bt.h"

#include "alert_dialog_page.h"
#include "bles/ble_server.h"

#define TAG "temp-page"
#define TEMP_DATA_TIMEOUT_MS 30000

// 温度
static uint16_t temp[] = {0xC2CE, 0xC8B6, 0x00};
// ℃
static uint16_t temp_f[] = {0xE6A1, 0x00};

// 湿度
static uint16_t hum[] = {0xAACA, 0xC8B6, 0x00};
// %
static uint16_t hum_f[] = {0x25, 0x00};

static float temperature;
static float humility;
static bool sht31_data_valid = false;
static uint32_t lst_read_tick;

static void temp_sensor_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    sht_data_t *data = NULL;
    switch (event_id) {
        case SHT_SENSOR_UPDATE:
            data = (sht_data_t *) event_data;
            ESP_LOGI(TAG, "temp: %f, hum: %f", data->temp, data->hum);
            temperature = data->temp;
            humility = data->hum;

            lst_read_tick = xTaskGetTickCount();

            if (sht31_data_valid == false) {
                ESP_LOGI(TAG, "temp current is invalid request update...");
                page_manager_request_update(false);
            }
            sht31_data_valid = true;
            break;
        case SHT_SENSOR_READ_FAILED:
            ESP_LOGE(TAG, "read temp and hum failed!");
            break;
        default:
            break;
    }
}

void temperature_page_on_create(void *args) {
    ESP_LOGI(TAG, "=== on create ===");
    esp_event_handler_register(
            BIKE_TEMP_HUM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
            temp_sensor_event_handler, NULL);
    sht40_init();
    sht31_data_valid = sht40_get_temp_hum(&temperature, &humility);
    lst_read_tick = xTaskGetTickCount();
}

void temperature_page_on_destroy(void *args) {
    ESP_LOGI(TAG, "=== on destroy ===");
    esp_event_handler_unregister(BIKE_TEMP_HUM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                 temp_sensor_event_handler);
}

void temperature_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    ESP_LOGI(TAG, "=== on draw ===");
    epd_paint_clear(epd_paint, 0);

    if (pdTICKS_TO_MS(xTaskGetTickCount() - lst_read_tick) >= TEMP_DATA_TIMEOUT_MS) {
        if (sht40_get_temp_hum(&temperature, &humility) == ESP_OK) {
            sht31_data_valid = true;
            lst_read_tick = xTaskGetTickCount();
        }
    }

    //epd_paint_draw_string_at(epd_paint, 167, 2, (char *)temp, &Font_HZK16, 1);
    digi_view_t *temp_label = digi_view_create(44, 7, 2);

    if (sht31_data_valid) {
        if (temperature > 100) {
            temperature = 100;
        }
        bool is_minus = temperature < 0;
        epd_paint_draw_string_at(epd_paint, 183, 24, (char *) temp_f, &Font_HZK16, 1);
        digi_view_set_text(temp_label, (int) temperature, 2, (int) (temperature * 10 + (is_minus ? -0.5f : 0.5f)) % 10,
                           1);
        digi_view_draw(8, 24, temp_label, epd_paint, loop_cnt);
    } else {
        digi_view_draw_ee(8, 24, temp_label, epd_paint, 3, loop_cnt);
    }

    digi_view_deinit(temp_label);

    //epd_paint_draw_string_at(epd_paint, 167, 130, (char *)hum, &Font_HZK16, 1);
    digi_view_t *hum_label = digi_view_create(22, 3, 2);
    if (sht31_data_valid) {
        if (humility < 0) {
            humility = 0;
        } else if (humility > 99) {
            humility = 99;
        }
        epd_paint_draw_string_at(epd_paint, 183, 172, (char *) hum_f, &Font_HZK16, 1);
        digi_view_set_text(hum_label, (int) humility, 2, (int) (humility * 10 + 0.5f) % 10, 1);
        digi_view_draw(102, 144, hum_label, epd_paint, loop_cnt);
    } else {
        digi_view_draw_ee(102, 144, temp_label, epd_paint, 3, loop_cnt);
    }
    digi_view_deinit(hum_label);

    // battery
    uint8_t icon_x = 4;
    battery_view_t *battery_view = battery_view_create(battery_get_level(), 26, 16);
    battery_view_draw(battery_view, epd_paint, icon_x, 183);
    battery_view_deinit(battery_view);
    icon_x += 30;

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        // ble icon
        epd_paint_draw_bitmap(epd_paint, icon_x, 183, 11, 16,
                              (uint8_t *) icon_ble_bmp_start,
                              icon_ble_bmp_end - icon_ble_bmp_start, 1);
        icon_x += 15;
    }
}

bool temperature_page_key_click_handle(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_FN_LONG_CLICK: {
            // show alert dialog
            ble_server_init();

            static alert_dialog_arg_t alert_dialog_arg = {
                    .title_label = (char *) text_ble_on,
                    .auto_close_ms = 5000
            };
            page_manager_show_menu("alert-dialog", &alert_dialog_arg);
            page_manager_request_update(false);
            return true;
        }
        default:
            break;
    }
    return false;
}


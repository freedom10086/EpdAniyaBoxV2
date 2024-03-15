//
// Created by yang on 2023/12/22.
//

#include "esp_wifi.h"

#include "data_time_page.h"

#include "common_utils.h"
#include "lcd/epdpaint.h"
#include "view/digi_view.h"
#include "view/battery_view.h"

#include "temperature_page.h"
#include "sht31.h"
#include "battery.h"
#include "static/static.h"
#include "page_manager.h"
#include "rx8025t.h"
#include "alert_dialog_page.h"
#include "bles/ble_server.h"
#include "static/static.h"

#define TAG "date-time-page"

static char date_time_page_draw_text_buf[32] = {0};

// 温度
static uint16_t temp[] = {0xC2CE, 0xC8B6, 0x00};
// ℃
static uint16_t temp_f[] = {0xE6A1, 0x00};

// 湿度
static uint16_t hum[] = {0xAACA, 0xC8B6, 0x00};
// %
static uint16_t hum_f[] = {0x25, 0x00};

static float temperature;
static bool temperature_valid = false;

static float humility;
static bool humility_valid = false;

static uint8_t _year, _month, _day, _week, _hour, _minute, _second;

static uint8_t check_week(uint8_t week) {
    if (week > 6) {
        return 6;
    }
    return week;
}

static void tem_hum_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    sht31_data_t *data = NULL;
    switch (event_id) {
        case SHT31_SENSOR_UPDATE:
            data = (sht31_data_t *) event_data;
            ESP_LOGI(TAG, "temp: %f, hum: %f", data->temp, data->hum);
            temperature = data->temp;
            humility = data->hum;

            if (temperature_valid == false) {
                ESP_LOGI(TAG, "temp current is invalid request update...");
                page_manager_request_update(false);
            }
            temperature_valid = true;
            humility_valid = true;
            break;
        case SHT31_SENSOR_READ_FAILED:
            ESP_LOGE(TAG, "read temp and hum failed!");
            break;
        default:
            break;
    }
}

static void date_time_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    rx8025t_data_t *data = NULL;
    switch (event_id) {
        case RX8025T_SENSOR_UPDATE:
            data = (rx8025t_data_t *) event_data;
            uint8_t old_minute = _minute;
            esp_err_t err = rx8025t_get_time(&_year, &_month, &_day, &_week, &_hour, &_minute, &_second);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "read time: %d-%d-%d %d:%d:%d week:%d", _year, _month, _day, _hour, _minute, _second,
                         _week);
                if (old_minute != _minute) {
                    page_manager_request_update(_minute % 5 == 0);
                }
            } else {
                ESP_LOGW(TAG, "read time failed");
            }
            break;
        default:
            break;
    }
}

void date_time_page_on_create(void *arg) {
    ESP_LOGI(TAG, "=== on create ===");

    sht31_init();

    esp_event_handler_register(BIKE_TEMP_HUM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                    tem_hum_event_handler, NULL);

    esp_event_handler_register(BIKE_DATE_TIME_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                    date_time_event_handler, NULL);
    ESP_LOGI(TAG, "=== created ===");
}

void date_time_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    ESP_LOGI(TAG, "=== on draw ===");
    epd_paint_clear(epd_paint, 0);

    rx8025t_get_time(&_year, &_month, &_day, &_week, &_hour, &_minute, &_second);

    if (_year >= 23 && _year < 30) {
        // draw date
        sprintf(date_time_page_draw_text_buf, "20%d.%d.%d", _year, _month, _day);
        epd_paint_draw_string_at(epd_paint, 0, 0, date_time_page_draw_text_buf, &Font16, 1);
        epd_paint_draw_string_at(epd_paint, 120, 0, (char *) text_week_label, &Font_HZK16, 1);
        uint8_t ok_week = check_week(_week);
        epd_paint_draw_string_at(epd_paint, 152, 0, (char *) text_week_num[ok_week], &Font_HZK16, 1);

        // draw time
        digi_view_t *time_label = digi_view_create(32, 6, 2);
        time_label->point_style = 1;
        digi_view_set_text(time_label, _hour, 2, _minute, 2);
        uint8_t time_label_width = digi_view_calc_width(time_label);

        uint8_t time_label_start_x = (epd_paint->width - time_label_width) / 2;
        digi_view_draw(time_label_start_x, 40, time_label, epd_paint, loop_cnt);
        digi_view_deinit(time_label);
    } else {
        uint8_t text_len = epd_paint_calc_string_width(epd_paint, (char *)text_datetime_need_set, &Font_HZK16);
        epd_paint_draw_string_at(epd_paint, (epd_paint->width - text_len) / 2, 100, (char *) text_datetime_need_set, &Font_HZK16, 1);
    }

    // battery
    battery_view_t *battery_view = battery_view_create(battery_get_level(), 26, 16);
    battery_view_draw(battery_view, epd_paint, 174, 0);
    battery_view_deinit(battery_view);

    if (sht31_get_temp_hum(&temperature, &humility)) {
        temperature_valid = true;
        humility_valid = true;
    }

    // temp
    digi_view_t *temp_label = digi_view_create(18, 3, 2);
    if (temperature_valid) {
        if (temperature > 100) {
            temperature = 100;
        }
        bool is_minus = temperature < 0;
        epd_paint_draw_string_at(epd_paint, 84, 184, (char *) temp_f, &Font_HZK16, 1);
        digi_view_set_text(temp_label, (int) temperature, 2, (int) (temperature * 10 + (is_minus ? -0.5f : 0.5f)) % 10,
                           1);
        digi_view_draw(16, 164, temp_label, epd_paint, loop_cnt);
    } else {
        digi_view_draw_ee(16, 164, temp_label, epd_paint, 3, loop_cnt);
    }

    digi_view_deinit(temp_label);

    // hum
    digi_view_t *hum_label = digi_view_create(18, 3, 2);
    if (humility_valid) {
        if (humility < 0) {
            humility = 0;
        } else if (humility > 99) {
            humility = 99;
        }
        epd_paint_draw_string_at(epd_paint, 184, 184, (char *) hum_f, &Font_HZK16, 1);
        digi_view_set_text(hum_label, (int) humility, 2, (int) (humility * 10 + 0.5f) % 10, 1);
        digi_view_draw(116, 164, hum_label, epd_paint, loop_cnt);
    } else {
        digi_view_draw_ee(116, 164, hum_label, epd_paint, 3, loop_cnt);
    }
    digi_view_deinit(hum_label);


#ifdef CONFIG_BT_BLUEDROID_ENABLED
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
        // ble icon
        epd_paint_draw_bitmap(epd_paint, icon_x, 183, 11, 16,
                              (uint8_t *) icon_ble_bmp_start,
                              icon_ble_bmp_end - icon_ble_bmp_start, 1);
        icon_x += 15;
    }
#endif
}

bool date_time_page_key_click(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_CANCEL_LONG_CLICK: {
            // show alert dialog
            ble_server_init();

            static alert_dialog_arg_t alert_dialog_arg = {
                    .title_label = (char *)text_ble_on,
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

void date_time_page_on_destroy(void *arg) {
    ESP_LOGI(TAG, "=== on destroy ===");
    esp_event_handler_unregister(BIKE_TEMP_HUM_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                      tem_hum_event_handler);

    esp_event_handler_unregister(BIKE_DATE_TIME_SENSOR_EVENT, ESP_EVENT_ANY_ID,
                                      date_time_event_handler);
}

int date_time_page_on_enter_sleep(void *arg) {
    return 60;
}

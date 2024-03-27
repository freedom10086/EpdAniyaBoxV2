//
// Created by yang on 2024/3/15.
//

#include "alarm_clock_page.h"

#include "view/switch_view.h"
#include "view/checkbox_view.h"
#include "view/number_input_view.h"
#include "view/button_view.h"
#include "view/view_group.h"
#include "static/static.h"
#include "page_manager.h"
#include "max31328.h"
#include "alert_dialog_page.h"
#include "bles/ble_server.h"

#define TAG "alarm-clock-page"

static view_t *enable_switch_view;
static view_t *week_checkbox_views[7];
static view_t *hour_number_input_view, *minute_number_input_view;
static view_t *save_button;

static max31328_alarm_t alarm;
static bool load_alarm_failed;
static view_group_view_t *view_group;

static void date_time_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case MAX31328_ALARM_CONFIG_UPDATE: {
            esp_err_t err = max31328_load_alarm1(&alarm);
            if (err != ESP_OK) {
                return;
            }

            switch_view_set_onoff(enable_switch_view, alarm.en);
            bool all_checked = (alarm.day_week >> 7) & 0x01;

            for (int i = 0; i < 7; ++i) {
                checkbox_view_set_checked(week_checkbox_views[i], all_checked || (alarm.day_week >> i) & 0x01);
            }

            number_input_view_set_value(hour_number_input_view, alarm.hour);
            number_input_view_set_value(minute_number_input_view, alarm.minute);

            page_manager_request_update(false);
            break;
        }
        default:
            break;
    }
}

static void on_save_btn_click(view_t *v) {
    alarm.en = switch_view_get_onoff(enable_switch_view);
    alarm.mode = 0;
    alarm.second = 0;
    alarm.minute = number_input_view_get_value(minute_number_input_view);
    alarm.hour = number_input_view_get_value(hour_number_input_view);;
    alarm.mode = ALARM_WEEK_MODE;

    alarm.day_week = 0;
    for (int i = 0; i < 7; ++i) {
        if (checkbox_view_get_checked(week_checkbox_views[i])) {
            alarm.day_week |= (0x01 << i);
        }
    }

    alarm.en2 = 0;

    esp_err_t err = max31328_set_alarm1(&alarm);
    ESP_LOGI(TAG, "save alarm en:%d, min:%d, hour:%d, daywee:%d", alarm.en, alarm.minute, alarm.hour, alarm.day_week);
    if (err == ESP_OK) {
        //page_manager_close_page();
        //page_manager_request_update(false);
    }
}

void alarm_clock_page_on_create(void *arg) {
    esp_err_t err = max31328_load_alarm1(&alarm);
    if (err != ESP_OK) {
        load_alarm_failed = true;
        return;
    }

    load_alarm_failed = false;
    enable_switch_view = switch_view_create(alarm.en);

    bool all_checked = (alarm.day_week >> 7) & 0x01;

    hour_number_input_view = number_input_view_create(alarm.hour, 0, 23, 1, &Font24);
    minute_number_input_view = number_input_view_create(alarm.minute, 0, 59, 1, &Font24);

    for (int i = 0; i < 7; ++i) {
        week_checkbox_views[i] = checkbox_view_create(all_checked || (alarm.day_week >> i) & 0x01);
    }

    save_button = button_view_create((char *) text_save, &Font_HZK16);
    view_set_click_cb(save_button, on_save_btn_click);

    // add to view group
    view_group = view_group_create();
    view_group_add_view(view_group, enable_switch_view);

    view_group_add_view(view_group, hour_number_input_view);
    view_group_add_view(view_group, minute_number_input_view);

    for (int i = 0; i < 7; ++i) {
        view_group_add_view(view_group, week_checkbox_views[i]);
    }

    view_group_add_view(view_group, save_button);

    esp_event_handler_register(BIKE_DATE_TIME_SENSOR_EVENT, MAX31328_ALARM_CONFIG_UPDATE,
                               date_time_event_handler, NULL);
}

void alarm_clock_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    if (load_alarm_failed) {
        epd_paint_draw_string_at(epd_paint, 6, 6, "load failed!", &Font16, 1);
        return;
    }

    uint8_t endx = epd_paint_draw_string_at(epd_paint, 6, 8, (char *) text_alarm_clock, &Font_HZK16, 1);
    switch_view_draw(enable_switch_view, epd_paint, endx + 5, 8);
    uint8_t endy = 8 + Font_HZK16.Height;

    if (switch_view_get_onoff(enable_switch_view)) {
        endy += 12;
        // hour
        uint8_t startx = epd_paint->width / 2 - (number_input_view_get_value(hour_number_input_view) >= 10
                                                 ? Font24.Width * 2 : Font24.Width) - 8;
        number_input_view_draw(hour_number_input_view, epd_paint, startx, endy);

        //:
        startx = epd_paint->width / 2 - Font24.Width / 2;
        epd_paint_draw_string_at(epd_paint, startx, endy, ":", &Font24, 1);

        // minute
        startx = epd_paint->width / 2 + 8;
        number_input_view_draw(minute_number_input_view, epd_paint, startx, endy);

        endy += Font24.Height;
        endy += 12;

        //repeat
        endx = epd_paint_draw_string_at(epd_paint, 6, endy, (char *) text_repeat, &Font_HZK16, 1);
        endx += 4;

        startx = endx;

        for (uint8_t w = 0; w < 7; w++) {
            endx = epd_paint_draw_string_at(epd_paint, endx, endy, (char *) text_week_label, &Font_HZK16, 1);
            endx = epd_paint_draw_string_at(epd_paint, endx, endy, (char *) text_week_num[w], &Font_HZK16, 1);
            endx = checkbox_view_draw(week_checkbox_views[w], epd_paint, endx, endy);
            endx += 10;

            if (w % 2 == 1) {
                endy += Font_HZK16.Height;
                endy += 8;
                endx = startx;
            }
        }
    }

    button_view_draw(save_button, epd_paint, epd_paint->width - 60, epd_paint->height - 30);
}

bool alarm_clock_page_key_click(key_event_id_t key_event_type) {
    if (!load_alarm_failed) {
        bool view_group_handled = view_group_handle_key_event(view_group, key_event_type);
        if (view_group_handled) {
            return true;
        }
    }

    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            if (load_alarm_failed) {
                page_manager_close_page();
                page_manager_request_update(false);
                return true;
            }
            break;
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
    return true;
}

void alarm_clock_page_on_destroy(void *arg) {
    esp_event_handler_unregister(BIKE_DATE_TIME_SENSOR_EVENT,
                                 MAX31328_ALARM_CONFIG_UPDATE,
                                 date_time_event_handler);

    switch_view_delete(enable_switch_view);

    for (int i = 0; i < 7; ++i) {
        checkbox_view_delete(week_checkbox_views[i]);
    }

    number_input_view_delete(hour_number_input_view);
    number_input_view_delete(minute_number_input_view);

    button_view_delete(save_button);

    view_group_delete(view_group);
}

int alarm_clock_page_on_enter_sleep(void *arg) {
    return DEFAULT_SLEEP_TS;
}
//
// Created by yang on 2024/3/15.
//

#include "alarm_clock_page.h"

#include "view/switch_view.h"
#include "view/checkbox_view.h"
#include "view/number_input_view.h"
#include "view/button_view.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "alarm-clock-page"

static switch_view_t *enable_switch_view;
static checkbox_view_t *week_checkbox_views[7];
static number_input_view_t *hour_number_input_view, *minute_number_input_view;
static button_view_t *save_button;

void alarm_clock_page_on_create(void *arg) {
    enable_switch_view = switch_view_create(true);

    for (int i = 0; i < 7; ++i) {
        week_checkbox_views[i] = checkbox_view_create(false);
    }

    hour_number_input_view = number_input_view_create(11, 0, 23, 1, &Font24);
    minute_number_input_view = number_input_view_create(26, 0, 23, 1, &Font24);

    save_button = button_view_create((char *) text_save, &Font_HZK16);
}

void alarm_clock_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    uint8_t endx = epd_paint_draw_string_at(epd_paint, 6, 8, (char *) text_alarm_clock, &Font_HZK16, 1);
    switch_view_draw(enable_switch_view, epd_paint, endx + 5, 8);
    uint8_t endy = 8 + Font_HZK16.Height;

    if (enable_switch_view->on) {
        endy += 12;
        // hour
        uint8_t startx = epd_paint->width / 2 - (hour_number_input_view->value >= 10 ? Font24.Width * 2 : Font24.Width)
                         - 8;
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
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            break;
        case KEY_FN_SHORT_CLICK:
            break;
        case KEY_UP_SHORT_CLICK:
            break;
        case KEY_DOWN_SHORT_CLICK:
            break;
        default:
            break;
    }
    return true;
}

void alarm_clock_page_on_destroy(void *arg) {
    switch_view_delete(enable_switch_view);

    for (int i = 0; i < 7; ++i) {
        checkbox_view_delete(week_checkbox_views[i]);
    }

    number_input_view_delete(hour_number_input_view);
    number_input_view_delete(minute_number_input_view);

    button_view_delete(save_button);
}

int alarm_clock_page_on_enter_sleep(void *arg) {
    return DEFAULT_SLEEP_TS;
}
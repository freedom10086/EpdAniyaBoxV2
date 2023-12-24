//
// Created by yang on 2023/12/24.
//

#include "battery_page.h"
#include "battery.h"
#include "page_manager.h"
#include "lcd/epd_lcd_ssd1680.h"

#define TAG "battery-page"

static char battery_page_draw_text_buf[64] = {0};

void battery_page_on_create(void *arg) {

}

void battery_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    int y = 8;

    sprintf(battery_page_draw_text_buf, "loop: %ld", loop_cnt);
    epd_paint_draw_string_at(epd_paint, 0, y, battery_page_draw_text_buf, &Font20, 1);
    y += 20;

    // battery
    int battery_voltage = battery_get_voltage();
    int8_t battery_level = battery_get_level();

    epd_paint_draw_string_at(epd_paint, 0, y, "battery:", &Font20, 1);
    y += 20;

    sprintf(battery_page_draw_text_buf, "%dmv %d%%", battery_voltage * 2, battery_level);
    epd_paint_draw_string_at(epd_paint, 16, y, battery_page_draw_text_buf, &Font16, 1);
    y += 18;

    sprintf(battery_page_draw_text_buf, "curving size: %ld", battery_get_curving_data_count());
    epd_paint_draw_string_at(epd_paint, 0, y, battery_page_draw_text_buf, &Font16, 1);
    y += 18;

    y = LCD_V_RES - 20;
    if (battery_is_curving()) {
        sprintf(battery_page_draw_text_buf, "curving...");
    } else {
        sprintf(battery_page_draw_text_buf, "click ok start curving");
    }
    epd_paint_draw_string_at(epd_paint, 0, y, battery_page_draw_text_buf, &Font16, 1);
    y += 18;
}

bool battery_page_key_click(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            if (!battery_is_curving()) {
                battery_start_curving();
                page_manager_request_update(false);
            }
            return true;
        case KEY_CANCEL_SHORT_CLICK:
            page_manager_close_page();
            page_manager_request_update(false);
            return true;
        default:
            break;
    }
    return false;
}

int battery_page_on_enter_sleep(void *args) {
    if (battery_is_curving()) {
        return NEVER_SLEEP_TS;
    }
    return DEFAULT_SLEEP_TS;
}
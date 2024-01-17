//
// Created by yang on 2023/12/24.
//

#include "battery_page.h"
#include "battery.h"
#include "page_manager.h"
#include "lcd/epd_lcd_ssd1680.h"
#include "bles/ble_server.h"
#include "confirm_menu_page.h"

#define TAG "battery-page"

static char battery_page_draw_text_buf[48] = {0};
static uint16_t confirm_start_curve_label[] = {0xAABF, 0xBCCA, 0xE7B5, 0xD8B3, 0xA3D0, 0xBCD7, 0x3F, 0x00};
static uint8_t click_ok_start_curve_label[] = {0xB5, 0xE3, 0xBB, 0xF7, 0x4F, 0x4B, 0xBF, 0xAA, 0xCA, 0xBC, 0xB5, 0xE7,
                                               0xB3, 0xD8, 0xD0, 0xA3, 0xD7, 0xBC, 0x00};

static void confirm_start_curving_dialog_callback(bool confirm) {
    if (confirm) {
        ble_server_init();
        battery_start_curving();
        page_manager_request_update(false);
    }
}

static confirm_menu_arg_t confirm_start_curving_arg = {
        .title_label = (char *) confirm_start_curve_label,
        .callback = confirm_start_curving_dialog_callback
};

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
        epd_paint_draw_string_at(epd_paint, 0, y, battery_page_draw_text_buf, &Font16, 1);
    } else {
        epd_paint_draw_string_at(epd_paint, 20, y, (char *) click_ok_start_curve_label, &Font_HZK16, 1);
    }
    y += 18;
}

bool battery_page_key_click(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            if (!battery_is_curving()) {
                page_manager_show_menu("confirm-alert", &confirm_start_curving_arg);
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

void battery_page_on_destroy(void *arg) {
    ble_server_stop_adv();
    ble_server_deinit();
}

int battery_page_on_enter_sleep(void *args) {
    if (battery_is_curving()) {
        ESP_LOGI(TAG, "battery is curving never sleep");
        return NEVER_SLEEP_TS;
    }

    if (ble_gap_conn_active()) {
        ESP_LOGI(TAG, "ble connect is active never sleep");
        return NEVER_SLEEP_TS;
    }

    ESP_LOGI(TAG, "battery page sleep %d", DEFAULT_SLEEP_TS);
    return DEFAULT_SLEEP_TS;
}
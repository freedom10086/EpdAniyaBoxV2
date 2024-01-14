//
// Created by yang on 2024/1/15.
//

#include "confirm_menu_page.h"

#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "common_utils.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "confirm_menu_page"

#define MENU_ITEM_COUNT 2
#define MENU_ITEM_HEIGHT 80
#define MENU_ITEM_WIDTH 120

static uint8_t current_index = 0;
static uint32_t switching_index = 0;

static confirm_menu_arg_t *confirm_menu_arg = NULL;

void confirm_menu_page_on_create(void *arg) {
    ESP_LOGI(TAG, "menu_page_on_create");
    confirm_menu_arg = (confirm_menu_arg_t *) arg;
    ESP_LOGI(TAG, "confirm_menu_arg == null ? %d, callback == null ? :%d", confirm_menu_arg == NULL ? 1 : 0,
             (confirm_menu_arg == NULL || confirm_menu_arg->callback == NULL) ? 1 : 0);
    current_index = 0;
    switching_index = 0;
}

void confirm_menu_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    uint8_t start_y = (LCD_V_RES - MENU_ITEM_HEIGHT) / 2;
    uint8_t start_x = (LCD_H_RES - MENU_ITEM_WIDTH) / 2;

    epd_paint_clear_range(epd_paint, start_x - 1, start_y - 1, MENU_ITEM_WIDTH + 2, MENU_ITEM_HEIGHT + 2, 0);
    // draw rec

    epd_paint_draw_rectangle(epd_paint, start_x, start_y, start_x + MENU_ITEM_WIDTH, start_y + MENU_ITEM_HEIGHT, 1);

    // draw title 4
    char *title_label;
    if (confirm_menu_arg == NULL || confirm_menu_arg->title_label == NULL) {
        title_label = "Confirm?";
    } else {
        title_label = confirm_menu_arg->title_label;
    }
    epd_paint_draw_string_at(epd_paint, start_x + 4, start_y + 8, title_label, &Font_HZK16, 1);

    // 4 + 16 + 4
    // two btn
    epd_paint_draw_rectangle(epd_paint, start_x + 8, start_y + 36, start_x + 8 + 45, start_y + 36 + 21, 1);
    epd_paint_draw_rectangle(epd_paint, start_x + 64, start_y + 36, start_x + 64 + 45, start_y + 36 + 21, 1);

    // two btn label
    char *cancel_label;
    if (confirm_menu_arg == NULL || confirm_menu_arg->cancel_label == NULL) {
        cancel_label = "x";
    } else {
        cancel_label = confirm_menu_arg->cancel_label;
    }
    epd_paint_draw_string_at(epd_paint, start_x + 8 + 2, start_y + 36 + 3, cancel_label, &Font_HZK16, 1);

    char *ok_label;
    if (confirm_menu_arg == NULL || confirm_menu_arg->confirm_label == NULL) {
        ok_label = "ok";
    } else {
        ok_label = confirm_menu_arg->confirm_label;
    }
    epd_paint_draw_string_at(epd_paint, start_x + 64 + 2, start_y + 36 + 3, ok_label, &Font_HZK16, 1);

    uint8_t x_offset = start_x + 8 + 2;
    if (current_index == 1) {
        x_offset = start_x + 64 + 2;
    }
    epd_paint_reverse_range(epd_paint, x_offset, start_y + 36 + 2, 42, 18);
}

void confirm_menu_page_after_draw(uint32_t loop_cnt) {
    switching_index = 0;
}

static void change_select(bool next) {
    switching_index = xTaskGetTickCount();
    current_index = (current_index + MENU_ITEM_COUNT + (next ? 1 : -1)) % MENU_ITEM_COUNT;
    page_manager_request_update(false);
}

bool confirm_menu_page_key_click(key_event_id_t key_event_type) {
    if (switching_index > 0 && xTaskGetTickCount() - switching_index < configTICK_RATE_HZ * 2) {
        ESP_LOGW(TAG, "handle pre click ignore %d", key_event_type);
        return true;
    }
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            if (confirm_menu_arg != NULL && confirm_menu_arg->callback != NULL) {
                confirm_menu_arg->callback(current_index == 1);
            }
            page_manager_close_menu();
            page_manager_request_update(false);
            break;
        case KEY_CANCEL_SHORT_CLICK:
            page_manager_close_menu();
            page_manager_request_update(false);
            break;
        case KEY_DOWN_SHORT_CLICK:
            change_select(true);
            break;
        case KEY_UP_SHORT_CLICK:
            change_select(false);
            break;
        default:
            return true;
    }
    // handle all key event
    return true;
}

void confirm_menu_page_on_destroy(void *arg) {

}
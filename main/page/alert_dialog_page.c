//
// Created by yang on 2024/1/15.
//

#include "alert_dialog_page.h"

#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "common_utils.h"
#include "static/static.h"
#include "page_manager.h"

#define TAG "alert_dialog_page"

#define MENU_ITEM_HEIGHT 70
#define MENU_ITEM_WIDTH 120

static alert_dialog_arg_t *dialog_arg = NULL;
static esp_timer_handle_t auto_close_timer_hdl = NULL;

static void auto_close_timer_callback(void *arg) {
    ESP_LOGI(TAG, "auto close dialog alert dialog created");
    page_manager_close_menu();
    page_manager_request_update(false);
}

void alert_dialog_page_on_create(void *arg) {
    dialog_arg = (alert_dialog_arg_t *) arg;
    ESP_LOGI(TAG, "alert dialog created");

    if (dialog_arg != NULL && dialog_arg->auto_close_ms > 0) {

        const esp_timer_create_args_t auto_close_timer_args = {
                .callback = &auto_close_timer_callback,
                /* argument specified here will be passed to timer callback function */
                .arg = NULL,
                .name = "auto-close-dialog"
        };

        ESP_ERROR_CHECK(esp_timer_create(&auto_close_timer_args, &auto_close_timer_hdl));
        ESP_ERROR_CHECK(esp_timer_start_once(auto_close_timer_hdl, dialog_arg->auto_close_ms * 1000));
    }
}

void alert_dialog_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    uint8_t start_y = (LCD_V_RES - MENU_ITEM_HEIGHT) / 2;
    uint8_t start_x = (LCD_H_RES - MENU_ITEM_WIDTH) / 2;

    epd_paint_clear_range(epd_paint, start_x - 1, start_y - 1, MENU_ITEM_WIDTH + 2, MENU_ITEM_HEIGHT + 2, 0);
    // draw rec

    epd_paint_draw_rectangle(epd_paint, start_x, start_y, start_x + MENU_ITEM_WIDTH, start_y + MENU_ITEM_HEIGHT, 1);

    // draw title 4
    char *title_label;
    if (dialog_arg == NULL || dialog_arg->title_label == NULL) {
        title_label = "Notice!";
    } else {
        title_label = dialog_arg->title_label;
    }
    epd_paint_draw_string_at(epd_paint, start_x + 4, start_y + 8, title_label, &Font_HZK16, 1);


    // btn
    char *btn_label;
    if (dialog_arg == NULL || dialog_arg->btn_label == NULL) {
        btn_label = "OK";
    } else {
        btn_label = dialog_arg->btn_label;
    }
    uint8_t btn_label_width = epd_paint_calc_string_width(epd_paint, btn_label, &Font_HZK16);
    epd_paint_draw_string_at(epd_paint, (LCD_H_RES - btn_label_width) / 2, start_y + 36 + 3, btn_label,
                             &Font_HZK16, 1);

    epd_paint_draw_rectangle(epd_paint, (LCD_H_RES - btn_label_width) / 2 - 4, start_y + 36,
                             (LCD_H_RES + btn_label_width) / 2 + 4,start_y + 36 + 21, 1);


    epd_paint_reverse_range(epd_paint, (LCD_H_RES - btn_label_width) / 2 - 2, start_y + 36 + 2, btn_label_width + 5, 18);
}

void alert_dialog_page_after_draw(uint32_t loop_cnt) {

}

bool alert_dialog_page_key_click(key_event_id_t key_event_type) {
    if (dialog_arg != NULL && dialog_arg->callback != NULL) {
        dialog_arg->callback();
    }
    page_manager_close_menu();
    page_manager_request_update(false);
    return true;
}

void alert_dialog_page_on_destroy(void *arg) {
    dialog_arg = NULL;
    if (auto_close_timer_hdl != NULL) {
        esp_timer_stop(auto_close_timer_hdl);
        esp_timer_delete(auto_close_timer_hdl);
        auto_close_timer_hdl = NULL;
    }
}
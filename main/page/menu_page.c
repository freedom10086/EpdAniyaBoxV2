#include "esp_log.h"

#include "lcd/epd_lcd_ssd1680.h"
#include "common_utils.h"
#include "menu_page.h"
#include "static/static.h"
#include "page_manager.h"
#include "LIS3DH.h"

#define TAG "menu_page"

#define MENU_ITEM_COUNT 4
#define MENU_ITEM_HEIGHT 56
#define MENU_ITEM_WIDTH 50
#define MENU_AUTO_CLOSE_TIMEOUT_TS 20

RTC_DATA_ATTR static uint8_t current_index = 0;
RTC_DATA_ATTR static esp_timer_handle_t auto_close_timer_hdl = NULL;

static void auto_close_timer_callback(void *arg) {
    ESP_LOGI(TAG, "auto close menu triggered");
    page_manager_close_menu();
    page_manager_request_update(false);
}

void menu_page_on_create(void *arg) {
    ESP_LOGI(TAG, "menu_page_on_create");

    const esp_timer_create_args_t auto_close_timer_args = {
            .callback = &auto_close_timer_callback,
            /* argument specified here will be passed to timer callback function */
            .arg = NULL,
            .name = "auto-close-menu"
    };

    ESP_ERROR_CHECK(esp_timer_create(&auto_close_timer_args, &auto_close_timer_hdl));
    ESP_ERROR_CHECK(esp_timer_start_once(auto_close_timer_hdl, MENU_AUTO_CLOSE_TIMEOUT_TS * 1000 * 1000));
}

void menu_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    ESP_LOGI(TAG, "=== on draw ===");
    uint8_t starty = epd_paint->height - MENU_ITEM_HEIGHT;

    //ESP_LOGI(TAG, "menu_page_draw");
    epd_paint_clear_range(epd_paint, 0, starty, LCD_H_RES, LCD_V_RES, 0);

    // draw line
    epd_paint_draw_horizontal_line(epd_paint, 0, starty, LCD_H_RES, 1);
    epd_paint_draw_horizontal_line(epd_paint, 0, starty + MENU_ITEM_HEIGHT, LCD_H_RES, 1);

    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH, starty, MENU_ITEM_HEIGHT * 2, 1);
    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH * 2, starty, MENU_ITEM_HEIGHT * 2, 1);
    epd_paint_draw_vertical_line(epd_paint, MENU_ITEM_WIDTH * 3, starty, MENU_ITEM_HEIGHT * 2, 1);

    // draw icon
    // 0. home
    epd_paint_draw_bitmap(epd_paint, 8, starty + 4, 35, 32,
                          (uint8_t *) ic_home_bmp_start,
                          ic_home_bmp_end - ic_home_bmp_start, 1);
    epd_paint_draw_string_at(epd_paint, 9, starty + 38, (char *) text_home, &Font_HZK16, 1);

    // 1. image
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH + 7, starty + 4, 36, 32,
                          (uint8_t *) ic_image_bmp_start,
                          ic_image_bmp_end - ic_image_bmp_start, 1);
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH + 9, starty + 38, (char *) text_image, &Font_HZK16, 1);

    // 2. setting
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 2 + 8, starty + 4, 33, 32,
                          (uint8_t *) ic_setting_bmp_start,
                          ic_setting_bmp_end - ic_setting_bmp_start, 1);
    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 2 + 9, starty + 38, (char *) text_setting, &Font_HZK16, 1);

    // 3. close
    epd_paint_draw_bitmap(epd_paint, MENU_ITEM_WIDTH * 3 + 8, starty + 4, 32, 32,
                          (uint8_t *) ic_close_bmp_start,
                          ic_close_bmp_end - ic_close_bmp_start, 1);

    epd_paint_draw_string_at(epd_paint, MENU_ITEM_WIDTH * 3 + 9, starty + 38, (char *) text_close, &Font_HZK16, 1);

    // select item
    uint8_t startX = (current_index % 4) * MENU_ITEM_WIDTH + 1;
    epd_paint_reverse_range(epd_paint, startX + 1, starty + 2, MENU_ITEM_WIDTH - 3, MENU_ITEM_HEIGHT - 2);
}

void menu_page_after_draw(uint32_t loop_cnt) {

}

static void change_select(bool next) {
    current_index = (current_index + MENU_ITEM_COUNT + (next ? 1 : -1)) % MENU_ITEM_COUNT;
    int full_update = 0;
    common_post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, &full_update, sizeof(full_update));
}

void handle_setting_item_event() {
    page_manager_close_menu();
    if (current_index == 0) {
        // home page
        page_manager_switch_page("temperature", false);
    } else if (current_index == 1) {
        // image manager
        page_manager_switch_page("image", false);
    } else if (current_index == 2) {
        // setting
        page_manager_switch_page("setting-list", true);
    }
    page_manager_request_update(false);
}

bool menu_page_key_click(key_event_id_t key_event_type) {
    if (auto_close_timer_hdl != NULL) {
        esp_timer_restart(auto_close_timer_hdl, MENU_AUTO_CLOSE_TIMEOUT_TS * 1000 * 1000);
    }

    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            handle_setting_item_event();
            break;
        case KEY_FN_SHORT_CLICK:
            page_manager_close_menu();
            page_manager_request_update(false);
            break;
        case KEY_DOWN_SHORT_CLICK:
            if (lis3dh_get_direction() == LIS3DH_DIR_LEFT)  {
                change_select(false);
            } else {
                change_select(true);
            }
            break;
        case KEY_UP_SHORT_CLICK:
            if (lis3dh_get_direction() == LIS3DH_DIR_LEFT)  {
                change_select(true);
            } else {
                change_select(false);
            }
            break;
        default:
            return true;
    }
    return true;
}

void menu_page_on_destroy(void *arg) {
    if (auto_close_timer_hdl != NULL) {
        esp_timer_stop(auto_close_timer_hdl);
        esp_timer_delete(auto_close_timer_hdl);
        auto_close_timer_hdl = NULL;
    }
}
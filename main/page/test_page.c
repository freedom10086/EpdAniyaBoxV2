#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "common_utils.h"
#include "test_page.h"
#include "tools/encode.h"
#include "lcd/epdpaint.h"
#include "bles/ble_server.h"
#include "page_manager.h"

#include "view/switch_view.h"
#include "view/checkbox_view.h"
#include "view/slider_view.h"
#include "view/number_input_view.h"


/*********************
 *      DEFINES
 *********************/
#define TAG "test-page"

static switch_view_t *switch_view, *switch_view2;
static checkbox_view_t *checkbox_view, *checkbox_view2;
static slider_view_t *slider_view;
static number_input_view_t *number_input_view;

void test_page_on_create(void *arg) {
    switch_view = switch_view_create(1);
    switch_view2 = switch_view_create(0);

    checkbox_view = checkbox_view_create(true);
    checkbox_view2 = checkbox_view_create(false);

    slider_view = slider_view_create(30, 0, 100);

    number_input_view = number_input_view_create(21, 0, 23, 1, &Font24);
}

void test_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    //你好世界！ gbk encode
    // https://www.qqxiuzi.cn/bianma/zifuji.php
    uint8_t data[] = {0xC4, 0xE3, 0xBA, 0xC3, 0xCA, 0xC0, 0xBD, 0xE7, 0xA3, 0xA1, 0x00};
    epd_paint_draw_string_at(epd_paint, 4, 4, (char *) data, &Font_HZK16, 1);

    switch_view_draw(switch_view, epd_paint, 10, 30);
    switch_view_draw(switch_view2, epd_paint, 100, 30);

    checkbox_view_draw(checkbox_view, epd_paint, 10, 60);
    checkbox_view_draw(checkbox_view2, epd_paint, 50, 60);

    slider_view_draw(slider_view, epd_paint, 10, 100);

    number_input_view_draw(number_input_view, epd_paint, 10, 130);
}

bool test_page_key_click(key_event_id_t key_event_type) {
    //ble_server_init();
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            switch_view_toggle(switch_view);
            switch_view_toggle(switch_view2);

            checkbox_view_toggle(checkbox_view);
            checkbox_view_toggle(checkbox_view2);

            slider_view_set_value(slider_view, slider_view->value + 10);


            number_input_view_set_state(number_input_view, VIEW_STATE_SELECTED);

            page_manager_request_update(false);
            break;
        case KEY_FN_SHORT_CLICK:
            slider_view_set_value(slider_view, slider_view->value - 10);

            number_input_view_set_state(number_input_view, VIEW_STATE_FOCUS);

            page_manager_request_update(false);
            break;
        case KEY_UP_SHORT_CLICK:
            if (number_input_view->interface->state == VIEW_STATE_SELECTED) {
                number_input_view_set_value(number_input_view, number_input_view->value + 1);
            }
            page_manager_request_update(false);
            break;
        case KEY_DOWN_SHORT_CLICK:
            if (number_input_view->interface->state == VIEW_STATE_SELECTED) {
                number_input_view_set_value(number_input_view, number_input_view->value - 1);
            }
            page_manager_request_update(false);
            break;
        default:
            break;
    }
    return true;
}

void test_page_on_destroy(void *arg) {
    //ble_server_deinit();
    switch_view_delete(switch_view);
    switch_view_delete(switch_view2);

    checkbox_view_delete(checkbox_view);
    checkbox_view_delete(checkbox_view2);

    slider_view_delete(slider_view);

    number_input_view_delete(number_input_view);
}

int test_page_on_enter_sleep(void *arg) {
    return 3600;
}
//
// Created by yang on 2023/12/25.
//
#include "page_manager.h"
#include "lcd/epd_lcd_ssd1680.h"

#include "music_page.h"
#include "beep/beep.h"
#include "view/list_view.h"

#define TAG "music-page"

static list_view_t *list_view = NULL;

void music_page_on_create(void *arg) {
    beep_init(BEEP_MODE_RMT);

    list_view = list_vew_create(0, 0, 200, 200, &Font_HZK16);

    uint16_t hszy[] = {0xAABB, 0xA2C9, 0xAED6, 0xB5D4, 0x00}; // 华散之缘
    list_view_add_element(list_view, (char *) hszy);

    uint16_t xxx[] = {0xA1D0, 0xC7D0, 0xC7D0, 0x00}; // 小星星
    list_view_add_element(list_view, (char *) xxx);

    list_view_add_element(list_view, "ddddd");
    list_view_add_element(list_view, "eeeee");
    list_view_add_element(list_view, "fffff");
    list_view_add_element(list_view, "ggggg");
    list_view_add_element(list_view, "hhhhh");
    list_view_add_element(list_view, "iiiii");
    list_view_add_element(list_view, "jjjjj");
    list_view_add_element(list_view, "kkkkk");
    list_view_add_element(list_view, "lllll");
    list_view_add_element(list_view, "mmmmm");
    list_view_add_element(list_view, "nnnnn");
}

void music_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    list_vew_draw(list_view, epd_paint, loop_cnt);
}

bool music_page_key_click(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            //beep_start_play(music_score_hszy, sizeof(music_score_hszy) / sizeof(music_score_hszy[0]));
            return true;
        case KEY_CANCEL_SHORT_CLICK:
            page_manager_close_page();
            page_manager_request_update(false);
            return true;
        case KEY_UP_SHORT_CLICK:
            list_view_select_pre(list_view);
            page_manager_request_update(false);
            return true;
        case KEY_DOWN_SHORT_CLICK:
            list_view_select_next(list_view);
            page_manager_request_update(false);
            return true;
        default:

            break;
    }
    return false;
}

void music_page_on_destroy(void *arg) {
    list_view_deinit(list_view);
}

int music_page_on_enter_sleep(void *args) {
    return DEFAULT_SLEEP_TS;
}
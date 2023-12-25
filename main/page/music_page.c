//
// Created by yang on 2023/12/25.
//
#include "page_manager.h"
#include "lcd/epd_lcd_ssd1680.h"

#include "music_page.h"
#include "beep/beep.h"

#define TAG "music-page"

void music_page_on_create(void *arg) {
    beep_init(BEEP_MODE_RMT);
}

void music_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {

}

bool music_page_key_click(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            beep_start_play(music_score_hszy, sizeof(music_score_hszy) / sizeof(music_score_hszy[0]));
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

int music_page_on_enter_sleep(void *args) {
    return DEFAULT_SLEEP_TS;
}
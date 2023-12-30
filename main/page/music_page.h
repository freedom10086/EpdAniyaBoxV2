//
// Created by yang on 2023/12/25.
//

#ifndef HELLO_WORLD_MUSIC_PAGE_H
#define HELLO_WORLD_MUSIC_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void music_page_on_create(void *arg);

void music_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool music_page_key_click(key_event_id_t key_event_type);

int music_page_on_enter_sleep(void *args);

void music_page_on_destroy(void *arg);

#endif //HELLO_WORLD_MUSIC_PAGE_H

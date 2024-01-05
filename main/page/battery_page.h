//
// Created by yang on 2023/12/24.
//

#ifndef HELLO_WORLD_BATTERY_PAGE_H
#define HELLO_WORLD_BATTERY_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

void battery_page_on_create(void *arg);

void battery_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool battery_page_key_click(key_event_id_t key_event_type);

void battery_page_on_destroy(void *arg);

int battery_page_on_enter_sleep(void *args);

#endif //HELLO_WORLD_BATTERY_PAGE_H

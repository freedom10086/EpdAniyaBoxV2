//
// Created by yang on 2023/12/22.
//

#ifndef HELLO_WORLD_DATA_TIME_PAGE_H
#define HELLO_WORLD_DATA_TIME_PAGE_H

#include "lcd/epdpaint.h"
#include "key.h"
#include "lcd/display.h"

void date_time_page_on_create(void *arg);

void date_time_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool date_time_page_key_click(key_event_id_t key_event_type);

void date_time_page_on_destroy(void *arg);

int date_time_page_on_enter_sleep(void *arg);

#endif //HELLO_WORLD_DATA_TIME_PAGE_H

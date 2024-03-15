//
// Created by yang on 2024/3/15.
//

#ifndef ANIYA_BOX_V2_ALARM_CLOCK_PAGE_H
#define ANIYA_BOX_V2_ALARM_CLOCK_PAGE_H

#include "lcd/epdpaint.h"
#include "key.h"
#include "lcd/display.h"

void alarm_clock_page_on_create(void *arg);

void alarm_clock_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool alarm_clock_page_key_click(key_event_id_t key_event_type);

void alarm_clock_page_on_destroy(void *arg);

int alarm_clock_page_on_enter_sleep(void *arg);

#endif //ANIYA_BOX_V2_ALARM_CLOCK_PAGE_H

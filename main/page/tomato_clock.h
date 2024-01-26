//
// Created by yang on 2024/1/24.
//

#ifndef ANIYA_BOX_V2_TOMATO_CLOCK_H
#define ANIYA_BOX_V2_TOMATO_CLOCK_H

#include "lcd/epdpaint.h"
#include "key.h"
#include "lcd/display.h"

typedef enum {
    TOMATO_INIT = 0,
    TOMATO_SETTING_STUDY_TIME,
    TOMATO_SETTING_PLAY_TIME,
    TOMATO_SETTING_LOOP_COUNT,
    TOMATO_STUDYING,
    TOMATO_PLAYING,
    TOMATO_SUMMARY
} tomato_stage_t;

void tomato_page_on_create(void *arg);

void tomato_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool tomato_page_key_click(key_event_id_t key_event_type);

void tomato_page_on_destroy(void *arg);

int tomato_page_on_enter_sleep(void *arg);

#endif //ANIYA_BOX_V2_TOMATO_CLOCK_H

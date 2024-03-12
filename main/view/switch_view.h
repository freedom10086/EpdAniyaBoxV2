//
// Created by yang on 2024/3/12.
//

#ifndef ANIYA_BOX_V2_SWITCH_VIEW_H
#define ANIYA_BOX_V2_SWITCH_VIEW_H

#include <stdio.h>
#include <stdlib.h>

#include "lcd/epdpaint.h"

typedef void (*on_switch_change_cb)(uint8_t onoff);

typedef struct {
    uint8_t on; // 开关状态
    on_switch_change_cb cb;
} switch_view_t;

switch_view_t *switch_view_create(uint8_t onoff);

void switch_view_set_change_cb(switch_view_t *view, on_switch_change_cb cb);

// return endx
uint8_t switch_view_draw(switch_view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return new state
uint8_t switch_view_toggle(switch_view_t *view);

uint8_t switch_view_set_onoff(switch_view_t *view, uint8_t onoff);

void switch_view_delete(switch_view_t *view);

#endif //ANIYA_BOX_V2_SWITCH_VIEW_H

//
// Created by yang on 2024/3/12.
//

#ifndef ANIYA_BOX_V2_SWITCH_VIEW_H
#define ANIYA_BOX_V2_SWITCH_VIEW_H

#include <stdio.h>
#include <stdlib.h>

#include "lcd/epdpaint.h"
#include "view_common.h"

typedef struct {
    view_interface_t *interface;
    uint8_t on; // 开关状态
    view_on_value_change_cb cb;
} switch_view_t;

switch_view_t *switch_view_create(uint8_t onoff);

// return endx
uint8_t switch_view_draw(void *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return new state
uint8_t switch_view_toggle(switch_view_t *view);

uint8_t switch_view_set_onoff(switch_view_t *view, uint8_t onoff);

void switch_view_delete(void* view);

#endif //ANIYA_BOX_V2_SWITCH_VIEW_H

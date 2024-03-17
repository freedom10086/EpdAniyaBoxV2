//
// Created by yang on 2024/3/12.
//

#ifndef ANIYA_BOX_V2_SWITCH_VIEW_H
#define ANIYA_BOX_V2_SWITCH_VIEW_H

#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"

#include "lcd/epdpaint.h"
#include "view_common.h"

typedef struct {
    view_t v;
    uint8_t on; // 开关状态
    view_on_value_change_cb cb;
} switch_view_t;

view_t *switch_view_create(uint8_t onoff);

// return endx
uint8_t switch_view_draw(view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return new state
bool switch_view_toggle(view_t *view);

bool switch_view_set_onoff(view_t *view, uint8_t onoff);

bool switch_view_get_onoff(view_t *view);

void switch_view_delete(view_t* view);

#endif //ANIYA_BOX_V2_SWITCH_VIEW_H

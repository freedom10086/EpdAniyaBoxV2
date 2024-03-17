//
// Created by yang on 2024/3/12.
//

#ifndef ANIYA_BOX_V2_CHECKBOX_VIEW_H
#define ANIYA_BOX_V2_CHECKBOX_VIEW_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lcd/epdpaint.h"
#include "view_common.h"

typedef struct {
    view_t v;
    bool checked;
    view_on_value_change_cb cb;
} checkbox_view_t;

view_t *checkbox_view_create(bool checked);

// return endx
uint8_t checkbox_view_draw(view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return new state
bool checkbox_view_toggle(view_t *view);

bool checkbox_view_get_checked(view_t *view);

bool checkbox_view_set_checked(view_t *view, bool checked);

void checkbox_view_delete(view_t *view);

#endif //ANIYA_BOX_V2_CHECKBOX_VIEW_H

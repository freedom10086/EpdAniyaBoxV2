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
    bool checked;
    view_on_value_change_cb cb;
} checkbox_view_t;

checkbox_view_t *checkbox_view_create(bool checked);

void checkbox_view_set_change_cb(checkbox_view_t *view, view_on_value_change_cb cb);

// return endx
uint8_t checkbox_view_draw(checkbox_view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return new state
bool checkbox_view_toggle(checkbox_view_t *view);

bool checkbox_view_set_checked(checkbox_view_t *view, bool checked);

void checkbox_view_delete(checkbox_view_t *view);

#endif //ANIYA_BOX_V2_CHECKBOX_VIEW_H

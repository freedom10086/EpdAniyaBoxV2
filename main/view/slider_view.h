//
// Created by yang on 2024/3/12.
//

#ifndef ANIYA_BOX_V2_SLIDER_VIEW_H
#define ANIYA_BOX_V2_SLIDER_VIEW_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lcd/epdpaint.h"
#include "view_common.h"

typedef struct {
    int value;
    int min;
    int max;
    view_on_value_change_cb cb;
} slider_view_t;

slider_view_t *slider_view_create(int value, int min, int max);

void slider_view_set_change_cb(slider_view_t *view, view_on_value_change_cb cb);

// return endx
uint8_t slider_view_draw(slider_view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return old value
int slider_view_set_value(slider_view_t *view, int value);

void slider_view_delete(slider_view_t *view);

#endif //ANIYA_BOX_V2_SLIDER_VIEW_H

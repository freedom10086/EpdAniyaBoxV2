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
    view_t v;
    int value;
    int min;
    int max;
    view_on_value_change_cb cb;
} slider_view_t;

view_t *slider_view_create(int value, int min, int max);

// return endx
uint8_t slider_view_draw(view_t *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return old value
int slider_view_set_value(view_t *view, int value);

void slider_view_delete(view_t *view);

#endif //ANIYA_BOX_V2_SLIDER_VIEW_H

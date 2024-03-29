//
// Created by yang on 2024/3/13.
//

#ifndef ANIYA_BOX_V2_NUMBER_INPUT_VIEW_H
#define ANIYA_BOX_V2_NUMBER_INPUT_VIEW_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lcd/fonts.h"

#include "lcd/epdpaint.h"
#include "view_common.h"

typedef struct {
    view_t v;
    int value;
    int min;
    int max;
    int gap;
    sFONT *font;
} number_input_view_t;

view_t *number_input_view_create(int value, int min, int max, int gap, sFONT *font);

// return endx
uint8_t number_input_view_draw(view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return old value
int number_input_view_set_value(view_t *view, int value);

int number_input_view_get_value(view_t *view);

void number_input_view_delete(view_t *view);

#endif //ANIYA_BOX_V2_NUMBER_INPUT_VIEW_H

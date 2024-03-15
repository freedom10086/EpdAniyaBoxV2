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
    view_interface_t *interface;
    int value;
    int min;
    int max;
    int gap;
    sFONT *font;
} number_input_view_t;

number_input_view_t *number_input_view_create(int value, int min, int max, int gap, sFONT *font);

// return endx
uint8_t number_input_view_draw(void *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

// return old value
int number_input_view_set_value(number_input_view_t *view, int value);

void number_input_view_delete(number_input_view_t *view);

bool number_input_view_set_state(void *view, view_state_t state);

#endif //ANIYA_BOX_V2_NUMBER_INPUT_VIEW_H

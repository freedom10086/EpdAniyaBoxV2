//
// Created by yang on 2024/3/15.
//

#ifndef ANIYA_BOX_V2_BUTTON_VIEW_H
#define ANIYA_BOX_V2_BUTTON_VIEW_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lcd/fonts.h"

#include "lcd/epdpaint.h"
#include "view_common.h"

typedef struct {
    view_t v;
    char *label;
    sFONT *font;
} button_view_t;

view_t *button_view_create(char *label, sFONT *font);

// return endx
uint8_t button_view_draw(view_t *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

void button_view_delete(view_t *v);

#endif //ANIYA_BOX_V2_BUTTON_VIEW_H

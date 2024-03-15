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
    view_interface_t *interface;
    view_on_click_cb click_cb;
    char *label;
    sFONT *font;
} button_view_t;

button_view_t *button_view_create(char *label, sFONT *font);

// return endx
uint8_t button_view_draw(void *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

void button_view_delete(void *view);

bool button_view_set_state(void *view, view_state_t state);

void button_view_set_click_cb(button_view_t *view, view_on_click_cb cb);

#endif //ANIYA_BOX_V2_BUTTON_VIEW_H

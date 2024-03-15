#ifndef BATTERY_VIEW_H
#define BATTERY_VIEW_H

#include <stdio.h>
#include <stdlib.h>

#include "lcd/epdpaint.h"

typedef struct {
    int width;
    int height;

    int battery_level;
} battery_view_t;

battery_view_t *battery_view_create(int level, int width, int height);

void battery_view_draw(battery_view_t *battery_view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);

void battery_view_set_level(battery_view_t *battery_view, int level);

void battery_view_deinit(battery_view_t *battery_view);

#endif
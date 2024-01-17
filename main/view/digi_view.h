#ifndef DIGI_VIEW_H
#define DIGI_VIEW_H

#include <stdio.h>
#include <stdlib.h>

#include "lcd/epdpaint.h"

typedef struct {
    int8_t number; // 整数部分  18
    uint8_t number_len; // 整数部分长度 不够补0
    int8_t decimal; // 小数部分 9 = 18.9
    uint8_t decimal_len; // 小数长度 0 表示没有
    uint8_t point_style; // 小数点样式 0 单点 1 双点

    uint8_t digi_width; // 每个文字宽度
    uint8_t digi_thick; // 厚度
    uint8_t digi_gap; // 每个数码管间隔
} digi_view_t;

digi_view_t *digi_view_create(uint8_t digi_width, uint8_t digi_thick, uint8_t digi_gap);

void digi_view_set_text(digi_view_t *digi_view, int8_t number, uint8_t number_len,  int8_t decimal, uint8_t decimal_len);

uint8_t digi_view_calc_width(digi_view_t *digi_view);

// return endx
uint8_t digi_view_draw(uint8_t x, uint8_t y, digi_view_t *digi_view, epd_paint_t *epd_paint, uint32_t loop_cnt);

void digi_view_draw_ee(uint8_t x, uint8_t y, digi_view_t *digi_view, epd_paint_t *epd_paint, uint8_t ee_cnt, uint32_t loop_cnt);

void digi_view_deinit(digi_view_t *digi_view);


#endif
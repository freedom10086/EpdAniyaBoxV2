//
// Created by yang on 2024/1/15.
//

#ifndef HELLO_WORLD_CONFIRM_MENU_PAGE_H
#define HELLO_WORLD_CONFIRM_MENU_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

typedef void (*on_confirm_menu_callback)(bool confirm);

typedef struct {
    char * title_label;
    char * confirm_label;
    char * cancel_label;
    on_confirm_menu_callback callback;
} confirm_menu_arg_t;

void confirm_menu_page_on_create(void *arg);

void confirm_menu_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

void confirm_menu_page_after_draw(uint32_t loop_cnt);

bool confirm_menu_page_key_click(key_event_id_t key_event_type);

void confirm_menu_page_on_destroy(void *arg);

#endif //HELLO_WORLD_CONFIRM_MENU_PAGE_H

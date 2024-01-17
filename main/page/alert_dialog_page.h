//
// Created by yang on 2024/1/15.
//

#ifndef HELLO_WORLD_ALERT_DIALOG_PAGE_H
#define HELLO_WORLD_ALERT_DIALOG_PAGE_H

#include "lcd/epdpaint.h"
#include "lcd/display.h"

typedef void (*on_alert_dialog_callback)();

typedef struct {
    char * title_label;
    char * btn_label;
    uint16_t auto_close_ms; // 0:not auto close
    on_alert_dialog_callback callback;
} alert_dialog_arg_t;

void alert_dialog_page_on_create(void *arg);

void alert_dialog_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

void alert_dialog_page_after_draw(uint32_t loop_cnt);

bool alert_dialog_page_key_click(key_event_id_t key_event_type);

void alert_dialog_page_on_destroy(void *arg);

#endif //HELLO_WORLD_ALERT_DIALOG_PAGE_H

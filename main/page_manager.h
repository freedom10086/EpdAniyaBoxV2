#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "stdio.h"
#include "stdlib.h"
#include "lcd/epdpaint.h"
#include "key.h"

#define TEMP_PAGE_INDEX 0
#define IMAGE_PAGE_INDEX 1
#define DATE_TIME_PAGE_INDEX 2
#define TOMATO_PAGE_INDEX 3

#define HOME_PAGE_COUNT 4

typedef void (*on_draw_page_cb)(epd_paint_t *epd_paint, uint32_t loop_cnt);

typedef void (*on_create_page_cb)(void *args);

typedef void (*on_destroy_page_cb)(void *args);

// return true: stop key event pass
typedef bool (*key_click_handler)(key_event_id_t key_event_type);

typedef int (*on_enter_sleep_handler)(void *args);

typedef void (*after_draw_page_cb)(uint32_t loop_cnt);

typedef int (*get_prefer_sleep_ts_cb)(uint32_t loop_cnt);

typedef struct {
    char *page_name;
    int8_t parent_page_index;
    on_draw_page_cb on_draw_page;
    key_click_handler key_click_handler;
    on_create_page_cb on_create_page;
    on_destroy_page_cb on_destroy_page;
    on_enter_sleep_handler enter_sleep_handler;
    after_draw_page_cb after_draw_page;
} page_inst_t;

void page_manager_init(int8_t page_index);

int8_t page_manager_get_current_index();

bool page_manager_switch_page(char *page_name, bool push_stack);

bool page_manager_close_page();

page_inst_t page_manager_get_current_page();

bool page_manager_has_menu();

page_inst_t page_manager_get_current_menu();

void page_manager_show_menu(char *name, void *args);

void page_manager_close_menu();

// return sleep ts, -1 stop sleep, 0 never wake up by timer
int page_manager_enter_sleep(uint32_t loop_cnt);

void page_manager_request_update(uint32_t full_refresh);

#endif
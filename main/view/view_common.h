//
// Created by yang on 2024/3/13.
//

#ifndef ANIYA_BOX_V2_VIEW_COMMON_H
#define ANIYA_BOX_V2_VIEW_COMMON_H

#define VIEW_OUTLINE_GAP 3

#include "key.h"
#include "lcd/epdpaint.h"

typedef enum {
    VIEW_STATE_NORMAL = 0,
    VIEW_STATE_DISABLE,
    VIEW_STATE_FOCUS,
    VIEW_STATE_SELECTED
} view_state_t;

typedef struct view_t {
    view_state_t state;
    bool selectable;

    // inner
    bool (*key_event)(struct view_t* view, key_event_id_t event);
    uint8_t (*draw)(struct view_t* view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);
    void (*delete)(struct view_t* view);

    // public
    void (*view_on_click_cb)(struct view_t* view);
    void (*view_on_value_change_cb)(struct view_t* view, int value);
} view_t;

typedef void (*view_on_value_change_cb)(struct view_t* view, int value);
typedef void (*view_on_click_cb)(struct view_t* view);

void view_set_click_cb(view_t *view, view_on_click_cb cb);
void view_set_value_change_cb(view_t *view, view_on_value_change_cb cb);

#endif //ANIYA_BOX_V2_VIEW_COMMON_H

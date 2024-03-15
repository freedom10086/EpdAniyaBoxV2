//
// Created by yang on 2024/3/13.
//

#ifndef ANIYA_BOX_V2_VIEW_COMMON_H
#define ANIYA_BOX_V2_VIEW_COMMON_H

#define VIEW_OUTLINE_GAP 3

typedef void (*view_on_value_change_cb)(void* view, int value);
typedef void (*view_on_click_cb)(void* view);

typedef enum {
    VIEW_STATE_NORMAL = 0,
    VIEW_STATE_DISABLE,
    VIEW_STATE_FOCUS,
    VIEW_STATE_SELECTED
} view_state_t;

typedef struct {
    view_state_t state;
    uint8_t (*draw)(void* view, epd_paint_t *epd_paint, uint8_t x, uint8_t y);
    void (*delete)(void* view);
} view_interface_t;

#endif //ANIYA_BOX_V2_VIEW_COMMON_H

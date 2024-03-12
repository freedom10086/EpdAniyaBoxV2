//
// Created by yang on 2024/3/12.
//

#include "switch_view.h"

#include "esp_log.h"

#define TAG "switch_view"

#define SWITCH_VIEW_GAP 2
#define SWITCH_VIEW_HEIGHT 18
#define SWITCH_VIEW_WIDTH 42

switch_view_t *switch_view_create(uint8_t onoff) {
    switch_view_t *view = malloc(sizeof(switch_view_t));
    if (!view) {
        ESP_LOGE(TAG, "no memory for init switch_view");
        return NULL;
    }

    view->on = onoff;

    ESP_LOGI(TAG, "list view created");
    return view;
}

void switch_view_set_change_cb(switch_view_t *view, on_switch_change_cb cb) {
    view->cb = cb;
}

// return endx
uint8_t switch_view_draw(switch_view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    epd_paint_draw_rectangle(epd_paint, x, y, x + SWITCH_VIEW_WIDTH, y + SWITCH_VIEW_HEIGHT, 1);

    if (view->on) {
        epd_paint_draw_filled_rectangle(epd_paint, x + SWITCH_VIEW_GAP, y + SWITCH_VIEW_GAP,
                                        x + SWITCH_VIEW_WIDTH / 2 - SWITCH_VIEW_GAP,
                                        y + SWITCH_VIEW_HEIGHT - SWITCH_VIEW_GAP, 1);

        epd_paint_draw_string_at_position(epd_paint, x + SWITCH_VIEW_WIDTH/ 2, y,
                                          x + SWITCH_VIEW_WIDTH, y + SWITCH_VIEW_HEIGHT,
                                          "ON",&Font12, ALIGN_CENTER, ALIGN_CENTER, 1);
    } else {
        epd_paint_draw_filled_rectangle(epd_paint, x + SWITCH_VIEW_WIDTH/ 2, y + SWITCH_VIEW_GAP,
                                        x + SWITCH_VIEW_WIDTH - SWITCH_VIEW_GAP,
                                        y + SWITCH_VIEW_HEIGHT - SWITCH_VIEW_GAP, 1);

        epd_paint_draw_string_at_position(epd_paint, x, y,
                                          x + SWITCH_VIEW_WIDTH / 2, y + SWITCH_VIEW_HEIGHT,
                                          "OFF",&Font12, ALIGN_CENTER, ALIGN_CENTER, 1);
    }


    return x + SWITCH_VIEW_WIDTH;
}

// return new state
uint8_t switch_view_toggle(switch_view_t *view) {
    if (view->on) {
        view->on = 0;
    } else {
        view->on = 1;
    }
    return view->on;
}

uint8_t switch_view_set_onoff(switch_view_t *view, uint8_t onoff) {
    uint8_t before = view->on;
    view->on = onoff;
    return before;
}

void switch_view_delete(switch_view_t *view) {
    if (view != NULL) {
        free(view);
        view = NULL;
    }
}

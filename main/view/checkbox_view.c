//
// Created by yang on 2024/3/12.
//

#include "checkbox_view.h"

#include "esp_log.h"

#define TAG "checkbox_view"

#define SWITCH_VIEW_GAP 4
#define SWITCH_VIEW_HEIGHT 18
#define SWITCH_VIEW_WIDTH 18

checkbox_view_t *checkbox_view_create(bool checked) {
    checkbox_view_t *view = malloc(sizeof(checkbox_view_t));
    if (!view) {
        ESP_LOGE(TAG, "no memory for init checkbox_view");
        return NULL;
    }

    view->checked = checked;

    ESP_LOGI(TAG, "checkbox view created");
    return view;
}

void checkbox_view_set_change_cb(checkbox_view_t *view, on_checked_change_cb cb) {
    view->cb = cb;
}

// return endx
uint8_t checkbox_view_draw(checkbox_view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {

    epd_paint_draw_rectangle(epd_paint, x, y, x + SWITCH_VIEW_WIDTH, y + SWITCH_VIEW_HEIGHT, 1);
    epd_paint_draw_rectangle(epd_paint, x + 1, y + 1, x + SWITCH_VIEW_WIDTH - 1, y + SWITCH_VIEW_HEIGHT - 1, 1);

    if (view->checked) {
        epd_paint_draw_filled_rectangle(epd_paint, x + SWITCH_VIEW_GAP, y + SWITCH_VIEW_GAP,
                                        x + SWITCH_VIEW_WIDTH - SWITCH_VIEW_GAP,
                                        y + SWITCH_VIEW_HEIGHT - SWITCH_VIEW_GAP, 1);
    }

    return x + SWITCH_VIEW_WIDTH;
}

// return new state
bool checkbox_view_toggle(checkbox_view_t *view) {
    if (view->checked) {
        view->checked = 0;
    } else {
        view->checked = 1;
    }
    return view->checked;
}

bool checkbox_view_set_checked(checkbox_view_t *view, bool checked) {
    bool before = view->checked;
    view->checked = checked;
    return before;
}

void checkbox_view_delete(checkbox_view_t *view) {
    if (view != NULL) {
        free(view);
        view = NULL;
    }
}
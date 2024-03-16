//
// Created by yang on 2024/3/12.
//

#include "checkbox_view.h"

#include "esp_log.h"

#define TAG "checkbox_view"

#define CHECK_BOX_VIEW_GAP 4
#define CHECK_BOX_VIEW_HEIGHT 18
#define CHECK_BOX_VIEW_WIDTH 18

checkbox_view_t *checkbox_view_create(bool checked) {
    checkbox_view_t *view = malloc(sizeof(checkbox_view_t));
    view->interface = (view_interface_t *) malloc(sizeof(view_interface_t));
    view->interface->state = VIEW_STATE_NORMAL;
    view->interface->draw = checkbox_view_draw;
    view->interface->delete = checkbox_view_delete;

    view->checked = checked;

    ESP_LOGI(TAG, "checkbox view created");
    return view;
}

void checkbox_view_set_change_cb(checkbox_view_t *view, view_on_value_change_cb cb) {
    view->cb = cb;
}

// return endx
uint8_t checkbox_view_draw(void *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    checkbox_view_t *view = v;

    epd_paint_draw_rectangle(epd_paint, x, y, x + CHECK_BOX_VIEW_WIDTH, y + CHECK_BOX_VIEW_HEIGHT, 1);
    epd_paint_draw_rectangle(epd_paint, x + 1, y + 1, x + CHECK_BOX_VIEW_WIDTH - 1, y + CHECK_BOX_VIEW_HEIGHT - 1, 1);

    if (view->checked) {
        epd_paint_draw_filled_rectangle(epd_paint, x + CHECK_BOX_VIEW_GAP, y + CHECK_BOX_VIEW_GAP,
                                        x + CHECK_BOX_VIEW_WIDTH - CHECK_BOX_VIEW_GAP,
                                        y + CHECK_BOX_VIEW_HEIGHT - CHECK_BOX_VIEW_GAP, 1);
    }

    uint8_t endx = x + CHECK_BOX_VIEW_WIDTH;
    uint8_t endy = y + CHECK_BOX_VIEW_HEIGHT;

    switch (view->interface->state) {
        case VIEW_STATE_FOCUS:
            // draw doted outline
            epd_paint_draw_doted_rectangle(epd_paint, x - VIEW_OUTLINE_GAP, y - VIEW_OUTLINE_GAP,
                                           endx + VIEW_OUTLINE_GAP,
                                           endy + VIEW_OUTLINE_GAP, 1);
            break;
        case VIEW_STATE_SELECTED:
            // draw solid outline
            epd_paint_draw_rectangle(epd_paint, x - VIEW_OUTLINE_GAP, y - VIEW_OUTLINE_GAP,
                                     endx + VIEW_OUTLINE_GAP,
                                     endy + VIEW_OUTLINE_GAP, 1);
            break;
        default:
            break;
    }

    return x + CHECK_BOX_VIEW_WIDTH;
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

void checkbox_view_delete(void *v) {
    if (v != NULL) {
        checkbox_view_t *view = v;
        if (view != NULL) {
            free(view);
            view = NULL;
        }
    }
}
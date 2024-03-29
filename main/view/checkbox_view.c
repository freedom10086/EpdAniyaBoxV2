//
// Created by yang on 2024/3/12.
//

#include "checkbox_view.h"

#include "esp_log.h"

#define TAG "checkbox_view"

#define CHECK_BOX_VIEW_GAP 4
#define CHECK_BOX_VIEW_HEIGHT 18
#define CHECK_BOX_VIEW_WIDTH 18

static bool key_event(view_t *v, key_event_id_t event) {
    if (event == KEY_OK_SHORT_CLICK) {
        bool checked = checkbox_view_toggle(v);
        if (v->view_on_value_change_cb != NULL) {
            v->view_on_value_change_cb(v, checked);
        }
        return true;
    }
    return false;
}

view_t *checkbox_view_create(bool checked) {
    view_t *view = malloc(sizeof(checkbox_view_t));

    view->state = VIEW_STATE_NORMAL;
    view->selectable = false;
    view->draw = checkbox_view_draw;
    view->delete = checkbox_view_delete;
    view->key_event = key_event;

    ((checkbox_view_t *) view)->checked = checked;

    ESP_LOGI(TAG, "checkbox view created");
    return view;
}

// return endx
uint8_t checkbox_view_draw(view_t *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    checkbox_view_t *view = (checkbox_view_t *) v;

    epd_paint_draw_rectangle(epd_paint, x, y, x + CHECK_BOX_VIEW_WIDTH, y + CHECK_BOX_VIEW_HEIGHT, 1);
    epd_paint_draw_rectangle(epd_paint, x + 1, y + 1, x + CHECK_BOX_VIEW_WIDTH - 1, y + CHECK_BOX_VIEW_HEIGHT - 1, 1);

    if (view->checked) {
        epd_paint_draw_filled_rectangle(epd_paint, x + CHECK_BOX_VIEW_GAP, y + CHECK_BOX_VIEW_GAP,
                                        x + CHECK_BOX_VIEW_WIDTH - CHECK_BOX_VIEW_GAP,
                                        y + CHECK_BOX_VIEW_HEIGHT - CHECK_BOX_VIEW_GAP, 1);
    }

    uint8_t endx = x + CHECK_BOX_VIEW_WIDTH;
    uint8_t endy = y + CHECK_BOX_VIEW_HEIGHT;

    switch (v->state) {
        case VIEW_STATE_FOCUS:
            // draw doted outline
            epd_paint_draw_doted_rectangle(epd_paint, x - VIEW_OUTLINE_GAP, y - VIEW_OUTLINE_GAP,
                                           endx + VIEW_OUTLINE_GAP,
                                           endy + VIEW_OUTLINE_GAP, 1);
            break;
        default:
            break;
    }

    return x + CHECK_BOX_VIEW_WIDTH;
}

// return new state
bool checkbox_view_toggle(view_t *v) {
    checkbox_view_t *view = (checkbox_view_t *) v;
    if (view->checked) {
        view->checked = 0;
    } else {
        view->checked = 1;
    }
    return view->checked;
}

bool checkbox_view_get_checked(view_t *v) {
    checkbox_view_t *view = (checkbox_view_t *) v;
    return view->checked;
}

bool checkbox_view_set_checked(view_t *v, bool checked) {
    checkbox_view_t *view = (checkbox_view_t *) v;
    bool before = view->checked;
    view->checked = checked;
    return before;
}

void checkbox_view_delete(view_t *v) {
    if (v != NULL) {
        free(v);
        v = NULL;
    }
}
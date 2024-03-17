//
// Created by yang on 2024/3/12.
//

#include "switch_view.h"

#include "esp_log.h"

#define TAG "switch_view"

#define SWITCH_VIEW_GAP 2
#define SWITCH_VIEW_HEIGHT 18
#define SWITCH_VIEW_WIDTH 42

static bool key_event(view_t *v, key_event_id_t event) {
    if (event == KEY_OK_SHORT_CLICK) {
        switch_view_toggle(v);
        return true;
    }
    return false;
}

view_t *switch_view_create(uint8_t onoff) {
    view_t *view = malloc(sizeof(switch_view_t));

    view->state = VIEW_STATE_NORMAL;
    view->draw = switch_view_draw;
    view->delete = switch_view_delete;
    view->key_event = key_event;

    switch_view_t *v = (switch_view_t *)view;
    v->on = onoff;

    ESP_LOGI(TAG, "list view created");
    return view;
}

// return endx
uint8_t switch_view_draw(view_t *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    switch_view_t *view = (switch_view_t *)v;
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

    uint8_t endx = x + SWITCH_VIEW_WIDTH;
    uint8_t endy = y + SWITCH_VIEW_HEIGHT;

    switch (v->state) {
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

    return x + SWITCH_VIEW_WIDTH;
}

// return new state
bool switch_view_toggle(view_t *v) {
    switch_view_t *view = (switch_view_t *)v;
    if (view->on) {
        view->on = 0;
    } else {
        view->on = 1;
    }
    return view->on;
}

bool switch_view_set_onoff(view_t *v, uint8_t onoff) {
    switch_view_t *view = (switch_view_t *)v;
    uint8_t before = view->on;
    view->on = onoff;
    return before;
}

bool switch_view_get_onoff(view_t *v) {
    switch_view_t *view = (switch_view_t *)v;
    return view->on;
}

void switch_view_delete(view_t* view) {
    if (view != NULL) {
        free(view);
        view = NULL;
    }
}

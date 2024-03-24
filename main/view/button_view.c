//
// Created by yang on 2024/3/15.
//

#include "button_view.h"
#include "esp_log.h"

#define TAG "button_view"

#define BUTTON_VIEW_GAP 4

static bool key_event(view_t *v, key_event_id_t event) {
    button_view_t *view = (button_view_t *)v;
    if (event == (key_event_id_t)KEY_OK_SHORT_CLICK) {
        if (view->v.view_on_click_cb != NULL) {
            view->v.view_on_click_cb(v);
        }
    }
    return false;
}

view_t *button_view_create(char *label, sFONT *font) {
    view_t *view = malloc(sizeof(button_view_t));

    view->selectable = false;
    view->state = VIEW_STATE_NORMAL;
    view->draw = button_view_draw;
    view->delete = button_view_delete;
    view->key_event = key_event;

    button_view_t *button_view = (button_view_t *) view;
    button_view->label = label;
    button_view->font = font;

    return view;
}

// return endx
uint8_t button_view_draw(view_t *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    button_view_t *view = (button_view_t *)v;
    uint8_t btn_label_width = epd_paint_calc_string_width(epd_paint, view->label, view->font);

    epd_paint_draw_string_at(epd_paint, x + BUTTON_VIEW_GAP, y + BUTTON_VIEW_GAP, view->label, view->font, 1);

    epd_paint_draw_rectangle(epd_paint, x, y,
                             x + btn_label_width + BUTTON_VIEW_GAP * 2,
                             y + view->font->Height + BUTTON_VIEW_GAP * 2, 1);

    switch (v->state) {
        case VIEW_STATE_FOCUS:
            epd_paint_reverse_range(epd_paint, x + 2, y + 2,
                                    btn_label_width + BUTTON_VIEW_GAP * 2 - 3,
                                    view->font->Height + BUTTON_VIEW_GAP * 2 - 3);
            break;
        default:
            break;
    }

    return btn_label_width + 4;
}

void button_view_delete(view_t *v) {
    if (v != NULL) {
        free(v);
        v = NULL;
    }
}
//
// Created by yang on 2024/3/15.
//

#include "button_view.h"
#include "esp_log.h"

#define TAG "button_view"

#define BUTTON_VIEW_GAP 4

button_view_t *button_view_create(char *label, sFONT *font) {
    button_view_t *view = malloc(sizeof(button_view_t));
    view->interface = (view_interface_t *) malloc(sizeof(view_interface_t));
    view->interface->state = VIEW_STATE_NORMAL;

    view->interface->draw = button_view_draw;
    view->interface->delete = button_view_delete;

    view->label = label;
    view->font = font;

    return view;
}

// return endx
uint8_t button_view_draw(void *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    button_view_t *view = v;
    uint8_t btn_label_width = epd_paint_calc_string_width(epd_paint, view->label, view->font);

    epd_paint_draw_string_at(epd_paint, x + BUTTON_VIEW_GAP, y + BUTTON_VIEW_GAP, view->label, view->font, 1);

    epd_paint_draw_rectangle(epd_paint, x, y,
                             x + btn_label_width + BUTTON_VIEW_GAP * 2,
                             y + view->font->Height + BUTTON_VIEW_GAP * 2, 1);

    switch (view->interface->state) {
        case VIEW_STATE_SELECTED:
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

void button_view_delete(void *view) {
    if (view != NULL) {
        free(((button_view_t *) view)->interface);
        free(view);
        view = NULL;
    }
}

void button_view_set_click_cb(button_view_t *view, view_on_click_cb cb) {
    view->interface->click = cb;
}

bool button_view_set_state(void *view, view_state_t state) {
    button_view_t *v = view;
    if (state == v->interface->state) {
        return false;
    }
    v->interface->state = state;
    return true;
}
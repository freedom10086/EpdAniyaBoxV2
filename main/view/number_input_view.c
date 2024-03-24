//
// Created by yang on 2024/3/13.
//

#include "number_input_view.h"
#include "esp_log.h"

#define TAG "number_input_view"

static bool key_event(view_t *v, key_event_id_t event) {
    number_input_view_t *view = (number_input_view_t *) v;
    if (event == (key_event_id_t)KEY_UP_SHORT_CLICK) {
        view->value = view->value + view->gap;
        if (view->value > view->max) {
            view->value = view->min;
        }
        return true;
    } else if (event == (key_event_id_t)KEY_DOWN_SHORT_CLICK) {
        view->value = view->value - view->gap;
        if (view->value < view->min) {
            view->value = view->max;
        }
        return true;
    }
    return false;
}

view_t *number_input_view_create(int value, int min, int max, int gap, sFONT *font) {
    view_t *view = malloc(sizeof(number_input_view_t));

    view->state = VIEW_STATE_NORMAL;
    view->selectable = true;
    view->draw = number_input_view_draw;
    view->delete = number_input_view_delete;
    view->key_event = key_event;

    number_input_view_t *input_view = (number_input_view_t *) (view);

    input_view->gap = gap;
    input_view->min = min;
    input_view->max = max;
    input_view->value = value;
    input_view->font = font;

    if (input_view->value < min) {
        input_view->value = min;
    } else if (input_view->value > max) {
        input_view->value = max;
    }

    ESP_LOGI(TAG, "number_input_view created");
    return view;
}

// return endx
uint8_t number_input_view_draw(view_t *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    number_input_view_t *view = (number_input_view_t *) v;
    char buff[5] = {0};
    sprintf(buff, "%d", view->value);
    uint8_t endx = epd_paint_draw_string_at(epd_paint, x, y, buff, view->font, 1);
    uint8_t endy = y + view->font->Height;

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

    return endx;
}

// return old value
int number_input_view_set_value(view_t *v, int value) {
    number_input_view_t *view = (number_input_view_t *) v;
    int backup_value = view->value;
    view->value = value;
    if (view->value < view->min) {
        view->value = view->min;
    } else if (view->value > view->max) {
        view->value = view->max;
    }
    return backup_value;
}

int number_input_view_get_value(view_t *v) {
    number_input_view_t *view = (number_input_view_t *) v;
    return view->value;
}

void number_input_view_delete(view_t *view) {
    if (view != NULL) {
        free(view);
        view = NULL;
    }
}
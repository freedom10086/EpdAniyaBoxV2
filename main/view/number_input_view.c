//
// Created by yang on 2024/3/13.
//

#include "number_input_view.h"
#include "esp_log.h"

#define TAG "number_input_view"

number_input_view_t *number_input_view_create(int value, int min, int max, int gap, sFONT *font) {
    number_input_view_t *view = malloc(sizeof(number_input_view_t));
    if (!view) {
        ESP_LOGE(TAG, "no memory for init number_input_view");
        return NULL;
    }

    view->interface = (view_interface_t *) malloc(sizeof(view_interface_t));
    view->interface->state = VIEW_STATE_NORMAL;
    view->interface->draw = number_input_view_draw;

    view->gap = gap;
    view->min = min;
    view->max = max;
    view->value = value;
    view->font = font;

    if (view->value < min) {
        view->value = min;
    } else if (view->value > max) {
        view->value = max;
    }

    ESP_LOGI(TAG, "number_input_view created");
    return view;
}

// return endx
uint8_t number_input_view_draw(void *v, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    number_input_view_t *view = v;
    char buff[5] = {0};
    sprintf(buff, "%d", view->value);
    uint8_t endx = epd_paint_draw_string_at(epd_paint, x, y, buff, view->font, 1);
    uint8_t endy = y + view->font->Height;

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

    return endx;
}

// return old value
int number_input_view_set_value(number_input_view_t *view, int value) {
    int backup_value = view->value;
    view->value = value;
    if (view->value < view->min) {
        view->value = view->min;
    } else if (view->value > view->max) {
        view->value = view->max;
    }
    return backup_value;
}

bool number_input_view_set_state(void *view, view_state_t state) {
    number_input_view_t *v = view;
    if (state == v->interface->state) {
        return false;
    }
    v->interface->state = state;
    return true;
}

void number_input_view_delete(number_input_view_t *view) {
    free(view->interface);
    free(view);
    view = NULL;
}
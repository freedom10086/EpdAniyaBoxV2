//
// Created by yang on 2024/3/12.
//

#include "slider_view.h"

#include "esp_log.h"

#define TAG "slider_view"

#define SLIDER_VIEW_WIDTH 100
#define SLIDER_VIEW_HEIGHT 18
#define SLIDER_VIEW_GAP 2

slider_view_t *slider_view_create(int value, int min, int max) {
    slider_view_t *view = malloc(sizeof(slider_view_t));
    if (!view) {
        ESP_LOGE(TAG, "no memory for init switch_view");
        return NULL;
    }

    view->min = min;
    view->max = max;
    view->value = value;

    if (view->value < min) {
        view->value = min;
    } else if (view->value > max) {
        view->value = max;
    }

    ESP_LOGI(TAG, "list view created");
    return view;
}

void slider_view_set_change_cb(slider_view_t *view, on_value_change_cb cb) {
    view->cb = cb;
}

// return endx
uint8_t slider_view_draw(slider_view_t *view, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    epd_paint_draw_rectangle(epd_paint, x, y, x + SLIDER_VIEW_WIDTH, y + SLIDER_VIEW_HEIGHT, 1);

    // calc progress
    int total_len = view->max - view->min;
    int current_len = view->value - view->min;
    int total_pixel = SLIDER_VIEW_WIDTH - SLIDER_VIEW_GAP * 2;
    int progress_pixel = current_len * total_pixel / total_len;
    if (current_len > 0 && progress_pixel == 0) {
        progress_pixel = 1;
    }

    // draw value label
    char buff[5] = {0};
    sprintf(buff, "%d", view->value);

    uint8_t text_end_x = epd_paint_draw_string_at_position(epd_paint, x + SLIDER_VIEW_GAP, y,
                                                           x + SLIDER_VIEW_WIDTH - SLIDER_VIEW_GAP,
                                                           y + SLIDER_VIEW_HEIGHT,
                                                           buff, &Font12, ALIGN_END, ALIGN_CENTER, 1);
    uint8_t text_start_x = text_end_x - epd_paint_calc_string_width(epd_paint, buff, &Font12);
    if (progress_pixel > 0) {
        uint8_t full_progress_end_x = x + SLIDER_VIEW_GAP + progress_pixel;
        if (full_progress_end_x <= text_start_x) {
            epd_paint_draw_filled_rectangle(epd_paint, x + SLIDER_VIEW_GAP, y + SLIDER_VIEW_GAP,
                                            full_progress_end_x,
                                            y + SLIDER_VIEW_HEIGHT - SLIDER_VIEW_GAP,
                                            1);
        } else {
            epd_paint_draw_filled_rectangle(epd_paint, x + SLIDER_VIEW_GAP, y + SLIDER_VIEW_GAP,
                                            text_start_x - 1,
                                            y + SLIDER_VIEW_HEIGHT - SLIDER_VIEW_GAP,
                                            1);
            epd_paint_reverse_range(epd_paint, text_start_x, y + SLIDER_VIEW_GAP,
                                    full_progress_end_x - text_start_x + 1,
                                    SLIDER_VIEW_HEIGHT - SLIDER_VIEW_GAP * 2 + 1);
        }
    }

    return x + SLIDER_VIEW_WIDTH;
}

// return old value
int slider_view_set_value(slider_view_t *view, int value) {
    int backup_value = view->value;
    view->value = value;
    if (view->value < view->min) {
        view->value = view->min;
    } else if (view->value > view->max) {
        view->value = view->max;
    }
    return backup_value;
}

void slider_view_delete(slider_view_t *view) {
    free(view);
    view = NULL;
}

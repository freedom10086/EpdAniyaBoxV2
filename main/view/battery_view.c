#include "esp_log.h"

#include "battery_view.h"
#include "common_utils.h"

#define TAG "battery_view"

#define LINE_THICK 1

battery_view_t *battery_view_create(int level, int width, int height) {
    //ESP_LOGI(TAG, "init battery view");
    battery_view_t *view = malloc(sizeof(battery_view_t));
    if (!view) {
        ESP_LOGE(TAG, "no memory for init digi view");
        return NULL;
    }


    view->width = width;
    view->height = height;
    view->battery_level = level;

    //ESP_LOGI(TAG, "battery view created");
    return view;
}

void battery_view_draw(battery_view_t *battery_view, epd_paint_t *epd_paint, uint8_t x, uint8_t y) {
    //ESP_LOGI(TAG, "battery_view_draw %d", level);
    uint8_t head_w = max(1, battery_view->height / 8);
    uint8_t head_h = max(2, battery_view->height / 4);

    // head
    epd_paint_draw_filled_rectangle(epd_paint, x + battery_view->width - head_w,
                                    y + (battery_view->height - head_h) / 2,
                                    x + battery_view->width,
                                    y + (battery_view->height + head_h) / 2,
                                    1);

    // frame
    epd_paint_draw_rectangle(epd_paint, x, y,
                             x + battery_view->width - head_w,
                             y + battery_view->height,
                             1);

    if (battery_view->battery_level < 0) {
        epd_paint_draw_line(epd_paint,
                            x + battery_view->width - head_w - head_h,
                            y,
                            x + head_h,
                            y + battery_view->height,
                            1);
    } else {
        // center level
        uint8_t battery_total_pixel = battery_view->width - LINE_THICK * 2 - head_w - 2;
        uint8_t current_level_pixel = battery_view->battery_level * battery_total_pixel / 100;
        // ESP_LOGI(TAG, "battery view level %d", level);
        if (current_level_pixel > 0) {
            epd_paint_draw_filled_rectangle(epd_paint, x + LINE_THICK + 1,
                                            y + LINE_THICK + 1,
                                            x + LINE_THICK + 1 + current_level_pixel,
                                            y + battery_view->height - 1 - LINE_THICK,
                                            1);
        }
    }
}

void battery_view_set_level(battery_view_t *battery_view, int level) {
    battery_view->battery_level = level;
}

void battery_view_deinit(battery_view_t *battery_view) {
    if (battery_view != NULL) {
        free(battery_view);
        battery_view = NULL;
    }
}
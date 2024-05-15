//
// Created by yang on 2024/4/7.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lcd/epdpaint.h"
#include "battery.h"

#include "lcd/display.h"
#include "page_manager.h"
#include "spl06.h"
#include "static/static.h"

#include "pressure_sensor_page.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "pressure-page"

static char info_page_draw_text_buf[64] = {0};
static bool sensor_init_successful = false;
static spl06_event_data_t _event_data;

static void sensor_update_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case SPL06_SENSOR_UPDATE: {
            _event_data = *(spl06_event_data_t *) event_data;
            page_manager_request_update(false);
            break;
        }
        default:
            break;
    }
}

bool pressure_sensor_page_key_click(key_event_id_t key_event_type) {
    page_manager_close_page();
    page_manager_request_update(false);
    return true;
}

void pressure_sensor_page_on_create(void *arg) {
    sensor_init_successful = (spl06_init() == ESP_OK);
    if (sensor_init_successful) {
        spl06_start(false, 1500);

        esp_event_handler_register(PRESSURE_SENSOR_EVENT, SPL06_SENSOR_UPDATE,
                                   sensor_update_event_handler, NULL);
    }
}

void pressure_sensor_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    uint16_t y = 0;

    if (!sensor_init_successful) {
        sprintf(info_page_draw_text_buf, "Sensor Init Failed!");
        epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font20, 1);
    } else {
        uint8_t x = 0;

        y += 8;
        x = epd_paint_draw_string_at(epd_paint, x, y, (char *) text_pressure, &Font_HZK16, 1);
        sprintf(info_page_draw_text_buf, ": %.2fPa", _event_data.pressure);
        epd_paint_draw_string_at(epd_paint, x, y, info_page_draw_text_buf, &Font16, 1);
        y += 20;

        x = 0;
        x = epd_paint_draw_string_at(epd_paint, x, y, (char *) text_altitude, &Font_HZK16, 1);
        sprintf(info_page_draw_text_buf, ": %.2fm", _event_data.altitude);
        epd_paint_draw_string_at(epd_paint, x, y, info_page_draw_text_buf, &Font16, 1);
        y += 20;

        x = 0;
        x = epd_paint_draw_string_at(epd_paint, x, y, (char *) text_temperature, &Font_HZK16, 1);
        sprintf(info_page_draw_text_buf, ": %.2f", _event_data.temp);
        x = epd_paint_draw_string_at(epd_paint, x, y, info_page_draw_text_buf, &Font16, 1);
        epd_paint_draw_string_at(epd_paint, x, y, (char *) text_temp_f, &Font_HZK16, 1);
        y += 20;
    }
}

int pressure_sensor_page_on_enter_sleep(void *args) {
    return DEFAULT_SLEEP_TS;
}

void pressure_sensor_page_on_destroy(void *arg) {
    esp_event_handler_unregister(PRESSURE_SENSOR_EVENT, SPL06_SENSOR_UPDATE,
                                 sensor_update_event_handler);

    spl06_deinit();
}

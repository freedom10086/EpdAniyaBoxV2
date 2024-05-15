//
// Created by yang on 2024/5/13.
//

#ifndef ANIYA_BOX_V2_BH1750_H
#define ANIYA_BOX_V2_BH1750_H

#include <esp_event_base.h>
#include "esp_err.h"

ESP_EVENT_DECLARE_BASE(BIKE_LIGHT_SENSOR_EVENT);

typedef struct {
    float lux;
} bh1750_data_t;

/**
 * opt3001 event
 */
typedef enum {
    BH1750_SENSOR_INIT_FAILED,
    BH1750_SENSOR_UPDATE,
    BH1750_SENSOR_READ_FAILED,
} bh1750_event_id_t;

/**
 * bh1750 mode
 */
typedef enum {
    BH1750_MODE_CONTINUE_H = 0b00010000,
    BH1750_MODE_CONTINUE_H2 = 0b00010001,
    BH1750_MODE_CONTINUE_L = 0b00010011,
    BH1750_MODE_SINGLE_H = 0b00100000,
    BH1750_MODE_SINGLE_H2 = 0b00100001,
    BH1750_MODE_SINGLE_L = 0b00100011,
} bh1750_measure_mode_t;

esp_err_t bh1750_init();
void bh1750_deinit();

// read most recent result
esp_err_t bh1750_start_measure(bh1750_measure_mode_t mode);
esp_err_t bh1750_read_lux(float *lux);

esp_err_t bh1750_reset();
esp_err_t bh1750_poweron();
esp_err_t bh1750_powerdown();

#endif //ANIYA_BOX_V2_BH1750_H

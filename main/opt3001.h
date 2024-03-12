//
// Created by yang on 2024/3/3.
//

#ifndef ANIYA_BOX_V2_OPT3001_H
#define ANIYA_BOX_V2_OPT3001_H

#include <esp_event_base.h>
#include "esp_err.h"

ESP_EVENT_DECLARE_BASE(BIKE_LIGHT_SENSOR_EVENT);

typedef struct {
    float lux;
} opt3001_data_t;

/**
 * opt3001 event
 */
typedef enum {
    OPT3001_SENSOR_INIT_FAILED,
    OPT3001_SENSOR_UPDATE,
    OPT3001_SENSOR_READ_FAILED,
} opt3001_event_id_t;

/**
 * opt3001 mode
 */

typedef enum {
    OPT3001_MODE_SHUTDOWN = 0,
    OPT3001_MODE_SINGLE_SHOT = 1,
    OPT3001_MODE_CONTINUE = 2
} opt3001_mode_t;

esp_err_t opt3001_init();
void opt3001_deinit();

// read most recent result
esp_err_t opt3001_read_lux(float *lux);
esp_err_t opt3001_read_low_limit(float *lux);
esp_err_t opt3001_read_high_limit(float *lux);

esp_err_t opt3001_read_mode(opt3001_mode_t *mode);

// 0 100ms 1 800ms
esp_err_t opt3001_start_once(uint8_t convertion_time);

// opt3001_start_once not need shutdown, only continue mode need
esp_err_t opt3001_shutdown();

#endif //ANIYA_BOX_V2_OPT3001_H

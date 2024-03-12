//
// Created by yang on 2024/3/3.
//

#ifndef ANIYA_BOX_V2_SGP30_H
#define ANIYA_BOX_V2_SGP30_H

#include <esp_event_base.h>
#include <esp_err.h>
#include "esp_types.h"

ESP_EVENT_DECLARE_BASE(BIKE_GASS_SENSOR_EVENT);

/**
 * usage
 *
 * sgp30_iaq_init
 *
 *  sgp30_set_humidity optional
 *  sgp30_set_baseline optional
 *
 * sgp30_iaq_measure
 * sgp30_set_humidity
 *
 * loop sgp30_iaq_measure, min gap 1s
 *
 */

typedef struct {
    float tvoc;
    float co2;
} sgp30_data_t;

/**
 * sgp30 event
 */
typedef enum {
    SGP30_SENSOR_INIT_FAILED,
    SGP30_SENSOR_UPDATE,
    SGP30_SENSOR_READ_FAILED,
} sgp30_event_id_t;

void sgp30_init();
void sgp30_deinit();

esp_err_t sgp30_iaq_init();
esp_err_t sgp30_iaq_measure();
esp_err_t sgp30_set_humidity(float temp, float hum);
//tvoc: 0 ppb to 60000 ppb, co2:400 ppm to 60000 ppm
esp_err_t sgp30_get_result(uint16_t *tvoc, uint16_t *co2);
esp_err_t sgp30_reset();

#endif //ANIYA_BOX_V2_SGP30_H

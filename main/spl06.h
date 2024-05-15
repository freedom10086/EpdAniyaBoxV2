#ifndef SPL06_H
#define SPL06_H

#include "esp_types.h"
#include "esp_event.h"
#include "common_utils.h"

#include <math.h>

ESP_EVENT_DECLARE_BASE(PRESSURE_SENSOR_EVENT);

#define SEA_LEVEL_PRESSURE 101325.0

/**
 * spl06 event
 */
typedef enum {
    SPL06_SENSOR_INIT_FAILED,
    SPL06_SENSOR_UPDATE,
    SPL06_SENSOR_READ_FAILED,
} spl06_event_id_t;

/**
 * spl06 event data
 */
typedef struct {
    float pressure;
    float altitude;
    float temp;
} spl06_event_data_t;

esp_err_t spl06_init();

void spl06_reset();
esp_err_t spl06_start(bool en_fifo, uint16_t interval_ms);
void spl06_stop();

float spl06_get_temperature();
float spl06_get_pressure();
void spl06_deinit();

#endif

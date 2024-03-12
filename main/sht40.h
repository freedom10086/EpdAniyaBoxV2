#ifndef SHT31_H
#define SHT31_H

#include <esp_event_base.h>

ESP_EVENT_DECLARE_BASE(BIKE_TEMP_HUM_SENSOR_EVENT);

typedef struct {
    float temp;
    float hum;
} sht_data_t;

/**
 * sht40 event
 */
typedef enum {
    SHT_SENSOR_INIT_FAILED,
    SHT_SENSOR_UPDATE,
    SHT_SENSOR_READ_FAILED,
} sht_event_id_t;

/**
 * sht40 event
 */
typedef enum {
    SHT_SENSOR_ACCURACY_LOW = 2, // max 1.7ms
    SHT_SENSOR_ACCURACY_MEDIUM = 5, // 4.5ms
    SHT_SENSOR_ACCURACY_HIGH = 10, //8.2ms
} sht_accuracy_t;

void sht40_init();
void sht40_deinit();
void sht40_reset();

esp_err_t sht40_start_measure(sht_accuracy_t accuracy);

esp_err_t sht40_get_temp_hum(float *temp, float *hum);

esp_err_t sht40_start_continues_measure(sht_accuracy_t accuracy);
void sht40_stop_continues_measure();
#endif
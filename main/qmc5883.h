//
// Created by yang on 2024/5/13.
//

#ifndef ANIYA_BOX_V2_QMC5883_H
#define ANIYA_BOX_V2_QMC5883_H

#include <esp_event_base.h>
#include "esp_err.h"

ESP_EVENT_DECLARE_BASE(BIKE_MAG_SENSOR_EVENT);

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    float azimuth;
} qmc5883_data_t;

typedef enum {
    QMC5883_MODE_STANDBY = 0,
    QMC5883_MODE_CONTINUE = 1
} qmc5883_mode_t;

typedef enum {
    QMC5883_ODR_10Hz = 0b00000000,
    QMC5883_ODR_50Hz = 0b00000100,
    QMC5883_ODR_100Hz = 0b00001000,
    QMC5883_ODR_200Hz = 0b00001100
} qmc5883_odr_t;

typedef enum {
    QMC5883_RNG_2G = 0b00000000,
    QMC5883_RNG_8G = 0b00010000
} qmc5883_rng_t;

typedef enum {
    QMC5883_OSR_512 = 0b00000000,
    QMC5883_OSR_256 = 0b01000000,
    QMC5883_OSR_128 = 0b10000000,
    QMC5883_OSR_64 = 0b11000000
} qmc5883_osr_t;

esp_err_t qmc5883_init();

void qmc5883_deinit();

esp_err_t qmc5883_reset();

esp_err_t qmc5883_set_mode(qmc5883_mode_t mode, qmc5883_odr_t odr, qmc5883_rng_t rng, qmc5883_osr_t osr);

esp_err_t qmc5883_read_raw(int16_t *x, int16_t *y, int16_t *z, uint8_t *overflow);

esp_err_t qmc5883_calibrate(uint32_t time_ms);

/**
 * Define the magnetic declination for accurate degrees.
 * https://www.magnetic-declination.com/
 *
 * @example
 * For: Londrina, PR, Brazil at date 2022-12-05
 * The magnetic declination is: -19º 43'
 * then: setMagneticDeclination(-19, 43);
 *
 * Shanghai, China -6° 35'
 */
void qmc5883_set_magnetic_declination(float magnetic_declination);

// from 0 to 360
esp_err_t qmc5883_get_azimuth(uint16_t *azimuth);

#endif //ANIYA_BOX_V2_QMC5883_H

//
// Created by yang on 2023/4/9.
//

#ifndef BLINK_LIS3DH_H
#define BLINK_LIS3DH_H

#include "driver/i2c.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(BIKE_MOTION_EVENT);

#define IMU_INT_1_GPIO -1
#define IMU_INT_ACTIVE_LEVEL 1

/**
 * LIS3DH_LOW_POWER_MODE : 16mg per deg
 * LIS3DH_NORMAL_MODE : 4mg per deg
 * LIS3DH_HIGH_RES_MODE : 1mg per deg
 */
typedef enum {
    LIS3DH_LOW_POWER_MODE,
    LIS3DH_NORMAL_MODE,
    LIS3DH_HIGH_RES_MODE
} lis3dh_mode_t;

typedef enum {
    LIS3DH_ACC_RANGE_2 = 0,
    LIS3DH_ACC_RANGE_4,
    LIS3DH_ACC_RANGE_8,
    LIS3DH_ACC_RANGE_16
} lis3dh_acc_range_t;

typedef enum {
    LIS3DH_ACC_SAMPLE_RATE_0 = 0, // power down mode
    LIS3DH_ACC_SAMPLE_RATE_1, // 1hz
    LIS3DH_ACC_SAMPLE_RATE_10,
    LIS3DH_ACC_SAMPLE_RATE_25,
    LIS3DH_ACC_SAMPLE_RATE_50,
    LIS3DH_ACC_SAMPLE_RATE_100,
    LIS3DH_ACC_SAMPLE_RATE_200,
    LIS3DH_ACC_SAMPLE_RATE_400,
} lis3dh_acc_sample_rage_t;

typedef enum {
    LIS3DH_ACC_EVENT_MOTION = 0,
    LIS3DH_ACC_EVENT_MOTION2,
    LIS3DH_DIRECTION_CHANGE,
} lis3dh_event_id;

typedef enum {
    LIS3DH_DIR_UNKNOWN = 0,
    LIS3DH_DIR_TOP,
    LIS3DH_DIR_LEFT,
    LIS3DH_DIR_BOTTOM,
    LIS3DH_DIR_RIGHT
} lis3dh_direction_t;

esp_err_t lis3dh_init(lis3dh_mode_t mode, lis3dh_acc_range_t acc_range, lis3dh_acc_sample_rage_t acc_sample_rate);

esp_err_t lis3dh_deinit();

esp_err_t
lis3dh_current_mode(lis3dh_mode_t *mode, lis3dh_acc_range_t *acc_range, lis3dh_acc_sample_rage_t *acc_sample_rate);

esp_err_t lis3dh_set_acc_range(bool high_res, lis3dh_acc_range_t acc_range);

esp_err_t lis3dh_set_sample_rate(bool low_pow, lis3dh_acc_sample_rage_t acc_sample_rate);

esp_err_t lis3dh_shutdown();

esp_err_t lis3dh_read_acc(float *accx, float *accy, float *accz);

lis3dh_direction_t lis3dh_calc_direction();

lis3dh_direction_t lis3dh_get_direction();

uint8_t lis3dh_config_motion_detect();

esp_err_t lis3dh_get_click_src(uint8_t *single_click, uint8_t *double_click);

esp_err_t lis3dh_get_int1_src(uint8_t *has_int);

esp_err_t lis3dh_disable_int();

#endif //BLINK_LIS3DH_H

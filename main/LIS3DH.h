//
// Created by yang on 2023/4/9.
//

#ifndef BLINK_LIS3DH_H
#define BLINK_LIS3DH_H

#include "driver/i2c.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(BIKE_MOTION_EVENT);

#define IMU_INT_1_GPIO 13


// 0011001 sa0 = 1 , default pull up
// 0011000 sa0 = 0
#define LIS3DH_ADDR 0B0011001

#define LIS3DH_REG_STATUS 0x07

// 0x33
#define LIS3DH_REG_WHO_AM_I 0x0F

#define LIS3DH_REG_CTRL_REG0 0x1E
#define LIS3DH_REG_CTRL_REG1 0x20
#define LIS3DH_REG_CTRL_REG2 0x21
#define LIS3DH_REG_CTRL_REG3 0x22
#define LIS3DH_REG_CTRL_REG4 0x23
#define LIS3DH_REG_CTRL_REG5 0x24
#define LIS3DH_REG_CTRL_REG6 0x25

#define LIS3DH_REG_OUT_X_L 0x28
#define LIS3DH_REG_OUT_X_H 0x29
#define LIS3DH_REG_OUT_Y_L 0x2A
#define LIS3DH_REG_OUT_Y_H 0x2B
#define LIS3DH_REG_OUT_Z_L 0x2C
#define LIS3DH_REG_OUT_Z_H 0x2D

#define LIS3DH_REG_INT1_CFG 0x30
#define LIS3DH_REG_INT1_SRC 0x31
#define LIS3DH_REG_INT1_THS 0x32
#define LIS3DH_REG_INT1_DURATION 0x33

#define LIS3DH_REG_INT2_CFG 0x34
#define LIS3DH_REG_INT2_SRC 0x35
#define LIS3DH_REG_INT2_THS 0x36
#define LIS3DH_REG_INT2_DURATION 0x37

#define LIS3DH_REG_CLICK_CFG 0x38
#define LIS3DH_REG_CLICK_SRC 0x39
#define LIS3DH_REG_CLICK_THS 0x3A
#define LIS3DH_REG_TIME_LIMIT 0x3B
#define LIS3DH_REG_TIME_LATENCY 0x3C
#define LIS3DH_REG_TIME_WINDOW 0x3D


// High Pass Filter values
#define LIS3DH_HPF_DISABLED               0x00
#define LIS3DH_HPF_AOI_INT1               0x01
#define LIS3DH_HPF_AOI_INT2               0x02
#define LIS3DH_HPF_CLICK                  0x04
#define LIS3DH_HPF_FDS                    0x08

#define LIS3DH_HPF_CUTOFF1                0x00
#define LIS3DH_HPF_CUTOFF2                0x10
#define LIS3DH_HPF_CUTOFF3                0x20
#define LIS3DH_HPF_CUTOFF4                0x30

#define LIS3DH_HPF_DEFAULT_MODE           0x00
#define LIS3DH_HPF_REFERENCE_SIGNAL       0x40
#define LIS3DH_HPF_NORMAL_MODE            0x80
#define LIS3DH_HPF_AUTORESET_ON_INTERRUPT 0xC0


///< Scalar to convert from 16-bit lsb to 10-bit and divide by 1k to
///< convert from milli-gs to gs
#define LIS3DH_LSB16_TO_KILO_LSB10  64000

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

#endif //BLINK_LIS3DH_H

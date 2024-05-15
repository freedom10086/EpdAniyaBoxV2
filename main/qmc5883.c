//
// Created by yang on 2024/5/13.
//

#include "qmc5883.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "math.h"
#include "esp_log.h"
#include "string.h"
#include "common_utils.h"

ESP_EVENT_DEFINE_BASE(BIKE_MAC_SENSOR_EVENT);

#define I2C_MASTER_TIMEOUT_MS       100
#define QMC5883_ADDR 0b0001101

#define QMC5883_REG_OUTX_L      0x00
#define QMC5883_REG_OUTX_H      0x01
#define QMC5883_REG_OUTY_L      0x02
#define QMC5883_REG_OUTY_H      0x03
#define QMC5883_REG_OUTZ_L      0x04
#define QMC5883_REG_OUTZ_H      0x05

//[0]:DRDY continuous measurement mode ready indicator auto reset when read any of 0x00-0x05
//[1]:OVL output value out of range not(-32768 and 32767)
//[2]:DOR data skip
#define QMC5883_REG_STATUS      0x06

#define QMC5883_REG_TEPM_L      0x07
#define QMC5883_REG_TEPM_H      0x08

// rw
//[1:0] MODE 0x00 standby 0x01 Continuous
//[3:2] ODR Output Data Rate 0x00 10hz, 0x01 50hz, 0x02 100hz, 0x03 200hz
//[5:4] RNG Full Scale, 0x00 2G, 0x01 8G
//[7:6] OSR Over Sample Ratio, 0x00 512, 0x01 256, 0x02 128, 0x03 64
#define QMC5883_REG_CONTROL     0x09

//[0] INT_ENB it will flag when new data is in Data Output Registers
//[6] ROL_PNT Enable pointer roll-over function
//[7] SOFT_RST Soft reset, restore default value of all registers.
#define QMC5883_REG_CONTROL2    0x0A
//
#define QMC5883_REG_SET_RESET_PERIOD    0x0B
// value is 0xff
#define QMC5883_REG_CHIP_ID     0x0D

static const char *TAG = "qmc5883";
extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;

RTC_DATA_ATTR bool _qmc5883_id_checked = false;
static bool qmc5883_inited = false;

static int16_t _raw[3];
static float _offset[3] = {0.0f, 0.0f, 0.0f};
static float _scale[3] = {1.0f, 1.0f, 1.0f};
static float _magnetic_declination_degrees = -6.58f;

static esp_err_t qmc_i2c_write_byte(uint8_t reg_addr, uint8_t write_data) {
    int ret;
    uint8_t write_buff[2];
    write_buff[0] = reg_addr;
    write_buff[1] = write_data;
    ret = i2c_master_transmit(dev_handle, write_buff, 2, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c data failed, for dev:%x addr:%x,  %s", QMC5883_ADDR, reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t qmc_i2c_read(uint8_t reg_addr, uint8_t *readdata, uint8_t read_len) {
    uint8_t u8_reg_addr[] = {reg_addr};
    esp_err_t err = i2c_master_transmit_receive(dev_handle,
                                                u8_reg_addr, 1, readdata, read_len,
                                                I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for dev:%x addr:%x,  %s", QMC5883_ADDR, reg_addr, esp_err_to_name(err));
        return err;
    }

    printf("read reg: %d \n", reg_addr);
    print_bytes(readdata, read_len);

    return ESP_OK;
}

esp_err_t qmc5883_init() {
    if (qmc5883_inited) {
        return ESP_OK;
    }

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = QMC5883_ADDR,
            .scl_speed_hz = 200000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

    if (!_qmc5883_id_checked) {
        // check device id
        uint16_t device_id;
        esp_err_t err = qmc_i2c_read(QMC5883_REG_CHIP_ID, &device_id, 1);
        if (err == ESP_OK) {
            if (device_id != 0xff) {
                ESP_LOGE(TAG, "device id not match 0x3001 read is:%x", device_id);
                return ESP_ERR_NOT_SUPPORTED;
            }
            _qmc5883_id_checked = true;
        } else {
            ESP_LOGE(TAG, "failed to read device id %s", esp_err_to_name(err));
            return err;
        }
    }

    qmc5883_inited = true;
    ESP_LOGI(TAG, "inited");
    return ESP_OK;
}

void qmc5883_deinit() {
    if (!qmc5883_inited) {
        return;
    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    qmc5883_inited = false;
}

esp_err_t qmc5883_reset() {
    if (!qmc5883_inited) {
        return ESP_OK;
    }

    return qmc_i2c_write_byte(QMC5883_REG_CONTROL2, 0x80);
}

esp_err_t qmc5883_set_mode(qmc5883_mode_t mode, qmc5883_odr_t odr, qmc5883_rng_t rng, qmc5883_osr_t osr) {
    if (!qmc5883_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    qmc_i2c_write_byte(QMC5883_REG_SET_RESET_PERIOD, 0x01);
    return qmc_i2c_write_byte(QMC5883_REG_CONTROL, mode | odr | rng | osr);
}

esp_err_t qmc5883_read_raw(int16_t *x, int16_t *y, int16_t *z, uint8_t *overflow) {
    if (!qmc5883_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t readbuff[6];
    esp_err_t err = qmc_i2c_read(QMC5883_REG_OUTX_L, readbuff, 6);
    if (err != ESP_OK) {
        return err;
    }

    _raw[0] = (int16_t) (readbuff[0] | readbuff[1] << 8);
    _raw[1] = (int16_t) (readbuff[2] | readbuff[3] << 8);
    _raw[2] = (int16_t) (readbuff[4] | readbuff[5] << 8);

    qmc_i2c_read(QMC5883_REG_STATUS, readbuff, 1);
    *overflow = readbuff[0] >> 1 & 0x01;

    *x = _raw[0];
    *y = _raw[1];
    *z = _raw[2];
    return ESP_OK;
}

esp_err_t qmc5883_calibrate(uint32_t time_ms) {
    if (!qmc5883_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    err = qmc5883_set_mode(QMC5883_MODE_CONTINUE, QMC5883_ODR_200Hz, QMC5883_RNG_8G, QMC5883_OSR_512);
    if (err != ESP_OK) {
        return err;
    }

    int32_t calibration_data[3][2] = {{65000, -65000},
                                      {65000, -65000},
                                      {65000, -65000}};
    int16_t x, y, z;
    uint8_t overflow = 0;

    int64_t start_time = esp_timer_get_time();
    while ((esp_timer_get_time() - start_time) < time_ms * 1000) {
        err = qmc5883_read_raw(&x, &y, &z, &overflow);
        if (err != ESP_OK) {
            return err;
        }
        if (x < calibration_data[0][0]) {
            calibration_data[0][0] = x;
        }
        if (x > calibration_data[0][1]) {
            calibration_data[0][1] = x;
        }

        if (y < calibration_data[1][0]) {
            calibration_data[1][0] = y;
        }
        if (y > calibration_data[1][1]) {
            calibration_data[1][1] = y;
        }

        if (z < calibration_data[2][0]) {
            calibration_data[2][0] = z;
        }
        if (z > calibration_data[2][1]) {
            calibration_data[2][1] = z;
        }
    }

    _offset[0] = (float) (calibration_data[0][0] + calibration_data[0][1]) / 2.0f;
    _offset[1] = (float) (calibration_data[1][0] + calibration_data[1][1]) / 2.0f;
    _offset[2] = (float) (calibration_data[2][0] + calibration_data[2][1]) / 2.0f;

    float x_avg_delta = (float) (calibration_data[0][1] - calibration_data[0][0]) / 2.0f;
    float y_avg_delta = (float) (calibration_data[1][1] - calibration_data[1][0]) / 2.0f;
    float z_avg_delta = (float) (calibration_data[2][1] - calibration_data[2][0]) / 2.0f;

    float avg_delta = (x_avg_delta + y_avg_delta + z_avg_delta) / 3;

    _scale[0] = avg_delta / x_avg_delta;
    _scale[1] = avg_delta / y_avg_delta;
    _scale[2] = avg_delta / z_avg_delta;

    return ESP_OK;
}

void qmc5883_set_magnetic_declination(float magnetic_declination) {
    _magnetic_declination_degrees = magnetic_declination;
}

esp_err_t qmc5883_get_azimuth(uint16_t *azimuth) {
    if (!qmc5883_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    int16_t x, y, z;
    uint8_t overflow;
    esp_err_t err = qmc5883_read_raw(&x, &y, &z, &overflow);
    if (err != ESP_OK) {
        return err;
    }

    float calibrated_x = (_raw[0] - _offset[0]) * _scale[0];
    float calibrated_y = (_raw[1] - _offset[1]) * _scale[1];
    //float calibrated_z = (_raw[2] - _offset[2]) * _scale[2];

    float heading = atan2(calibrated_y, calibrated_x) * 180.0f / 3.1415926f;
    heading += _magnetic_declination_degrees;
    *azimuth = ((uint16_t) (heading + 360)) % 360;
    return ESP_OK;
}
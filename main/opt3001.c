//
// Created by yang on 2024/3/3.
//

#include "opt3001.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "math.h"
#include "esp_log.h"
#include "string.h"

ESP_EVENT_DEFINE_BASE(BIKE_LIGHT_SENSOR_EVENT);

#define I2C_MASTER_TIMEOUT_MS       100
#define OPT_3001_ADDR 0b1000100

//15:12 -> E[3:0] 11->0 R[11:0]
#define OPT_3001_REG_RESULT 0x00

#define OPT_3001_REG_CONFIG 0x01
#define OPT_3001_REG_LOW_LIMIT 0x02
#define OPT_3001_REG_HIGH_LIMIT 0x03
#define OPT_3001_REG_MANUFACTURER_ID 0x7e
#define OPT_3001_REG_DEVICE_ID 0x7f

static const char *TAG = "opt3001";
extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;

RTC_DATA_ATTR bool _opt3001device_id_checked = false;

typedef union {
    uint16_t raw_data;
    struct {
        uint16_t r: 12;
        uint8_t e: 4;
    };
} opt3001_r_t;

typedef union {
    struct {
        uint8_t fault_count: 2;
        uint8_t mask_exponent: 1;
        uint8_t polarity: 1;
        uint8_t latch: 1;
        uint8_t flaglow: 1;
        uint8_t flaghigh: 1;
        uint8_t conversion_ready: 1;
        uint8_t overflow_flag: 1;
        uint8_t mode_of_conversion_operation: 2; // [00] shutdown 01 single-shot 10,11 continuous
        uint8_t convertion_time: 1; // Conversion time 0 100ms [1] 800ms
        uint8_t range_number: 4; // [1100] auto range mode
    };
    uint16_t raw_data;
} opt3001_config_t;

static opt3001_config_t opt3001_config;
static bool opt3001_config_read = false;

static bool opt3001_inited = false;

static esp_err_t opt_i2c_write(uint8_t reg_addr, uint16_t *write_data, size_t len) {
    int ret;
    uint8_t write_buff[1 + len * 2];

    write_buff[0] = reg_addr;
    memcpy(write_buff + 1, write_data, len * 2);

    ret = i2c_master_transmit(dev_handle, write_buff, 2 + len, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c data failed, for dev:%x addr:%x,  %s", OPT_3001_ADDR, reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t opt_i2c_read(uint8_t reg_addr, uint16_t *readdata, uint8_t read_len) {
    uint8_t replylen = read_len * 2;
    uint8_t replybuffer[replylen];

    uint8_t u8_reg_addr[] = {reg_addr};
    esp_err_t err = i2c_master_transmit_receive(dev_handle,
                                                u8_reg_addr, 1, replybuffer, replylen,
                                                I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for dev:%x addr:%x,  %s", OPT_3001_ADDR, reg_addr, esp_err_to_name(err));
        return err;
    }

    for (uint8_t i = 0; i < read_len; i++) {
        readdata[i] = (replybuffer[i * 2] << 8) | replybuffer[i * 2 + 1];
    }
    return ESP_OK;
}

esp_err_t opt3001_init() {
    if (opt3001_inited) {
        return ESP_OK;
    }

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = OPT_3001_ADDR,
            .scl_speed_hz = 200000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

    if (!_opt3001device_id_checked) {
        // check device id
        uint16_t device_id;
        esp_err_t err = opt_i2c_read(OPT_3001_REG_DEVICE_ID, &device_id, 1);
        if (err == ESP_OK) {
            if (device_id != 0x3001) {
                ESP_LOGE(TAG, "device id not match 0x3001 read is:%x", device_id);
                return ESP_ERR_NOT_SUPPORTED;
            }
            _opt3001device_id_checked = true;
        } else {
            ESP_LOGE(TAG, "failed to read device id %s", esp_err_to_name(err));
            return err;
        }
    }

    opt3001_inited = true;
    return ESP_OK;
}

void opt3001_deinit() {
    if (!opt3001_inited) {
        return;
    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    opt3001_inited = false;
}

esp_err_t opt3001_read_lux(float *lux) {
    esp_err_t err = opt3001_init();
    if (err != ESP_OK) {
        return err;
    }

    opt3001_r_t read_out;
    err = opt_i2c_read(OPT_3001_REG_RESULT, &read_out.raw_data, 1);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t e = read_out.e;
    uint16_t r = read_out.r;
    *lux = 0.01f * pow(2, e) * r;
    return ESP_OK;
}

esp_err_t opt3001_read_low_limit(float *lux) {
    esp_err_t err = opt3001_init();
    if (err != ESP_OK) {
        return err;
    }

    opt3001_r_t read_out;
    err = opt_i2c_read(OPT_3001_REG_LOW_LIMIT, &read_out.raw_data, 1);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t e = read_out.e;
    uint16_t r = read_out.r;
    *lux = 0.01f * pow(2, e) * r;
    return ESP_OK;
}

esp_err_t opt3001_read_high_limit(float *lux) {
    esp_err_t err = opt3001_init();
    if (err != ESP_OK) {
        return err;
    }

    opt3001_r_t read_out;
    err = opt_i2c_read(OPT_3001_REG_HIGH_LIMIT, &read_out.raw_data, 1);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t e = read_out.e;
    uint16_t r = read_out.r;
    *lux = 0.01f * pow(2, e) * r;
    return ESP_OK;
}

esp_err_t opt3001_read_config() {
    esp_err_t err = opt_i2c_read(OPT_3001_REG_CONFIG, &opt3001_config.raw_data, 1);
    if (err == ESP_OK) {
        opt3001_config_read = true;
    }
    return err;
}

esp_err_t opt3001_read_mode(opt3001_mode_t *mode) {
    esp_err_t err = opt3001_init();
    if (err != ESP_OK) {
        return err;
    }

    err = opt3001_read_config();
    if (err != ESP_OK) {
        return err;
    }

    switch (opt3001_config.mode_of_conversion_operation) {
        case 0:
            *mode = OPT3001_MODE_SHUTDOWN;
            return ESP_OK;
        case 1:
            *mode = OPT3001_MODE_SINGLE_SHOT;
            return ESP_OK;
        default:
            *mode = OPT3001_MODE_CONTINUE;
            return ESP_OK;
    }
}

esp_err_t opt3001_start_once(uint8_t convertion_time) {
    esp_err_t err = opt3001_init();
    if (err != ESP_OK) {
        return err;
    }

    if (!opt3001_config_read) {
        err = opt3001_read_config();
        if (err != ESP_OK) {
            return err;
        }
    }

    opt3001_config.range_number = 0b1100; // auto range mode
    opt3001_config.mode_of_conversion_operation = OPT3001_MODE_SINGLE_SHOT;
    opt3001_config.convertion_time = convertion_time;
    opt3001_config.latch = 1;

    return opt_i2c_write(OPT_3001_REG_CONFIG, &opt3001_config.raw_data, 1);
}

esp_err_t opt3001_shutdown() {
    esp_err_t err = opt3001_init();
    if (err != ESP_OK) {
        return err;
    }

    opt3001_mode_t mode;
    err = opt3001_read_mode(&mode);
    if (err != ESP_OK) {
        return err;
    }

    if (opt3001_config.mode_of_conversion_operation > 1) {
        opt3001_config.mode_of_conversion_operation = OPT3001_MODE_SHUTDOWN;
        return opt_i2c_write(OPT_3001_REG_CONFIG, &opt3001_config.raw_data, 1);
    }

    return ESP_OK;
}
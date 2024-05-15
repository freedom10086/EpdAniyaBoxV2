//
// Created by yang on 2024/5/13.
//

#include "bh1750.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "math.h"
#include "esp_log.h"
#include "string.h"
#include "common_utils.h"

ESP_EVENT_DEFINE_BASE(BIKE_LIGHT_SENSOR_EVENT);

#define I2C_MASTER_TIMEOUT_MS       150

// addr = 1 0b1011100
// addr = 0
#define BH1750_ADDR 0b0100011

#define BH1750_CMD_POWER_DOWN           0b00000000
#define BH1750_CMD_POWER_ON             0b00000001
#define BH1750_CMD_RESET                0b00000111
// 120ms 1lx res
#define BH1750_CMD_MODE_CONTINUE_HRES   0b00010000
// 120ms 0.5lux res
#define BH1750_CMD_MODE_CONTINUE_HRES2  0b00010001
// 16ms 4lux res
#define BH1750_CMD_MODE_CONTINUE_LRES   0b00010011
#define BH1750_CMD_MODE_SINGLE_HRES   0b00100000
#define BH1750_CMD_MODE_SINGLE_HRES2  0b00100001
#define BH1750_CMD_MODE_SINGLE_LRES   0b00100011

static const char *TAG = "bh1750";
extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;

static bool bh1750_inited = false;

static esp_err_t bh1750_i2c_write_cmd(uint8_t cmd) {
    int ret;
    uint8_t write_buff[1];
    write_buff[0] = cmd;
    ret = i2c_master_transmit(dev_handle, write_buff, 1, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c data failed, for dev:%x cmd:%x,  %s", BH1750_ADDR, cmd, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t bh1750_i2c_read(uint16_t *readdata) {
    uint8_t replylen = 2;
    uint8_t replybuffer[replylen];

    esp_err_t err = i2c_master_receive(dev_handle, replybuffer, replylen,
                                                I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for dev:%x %s", BH1750_ADDR, esp_err_to_name(err));
        return err;
    }
    print_bytes(replybuffer, replylen);
    *readdata = replybuffer[0] << 8 | replybuffer[1];
    return ESP_OK;
}

esp_err_t bh1750_init() {
    if (bh1750_inited) {
        return ESP_OK;
    }
    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = BH1750_ADDR,
            .scl_speed_hz = 200000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));
    bh1750_inited = true;
    ESP_LOGI(TAG, "inited");
    return ESP_OK;
}

void bh1750_deinit() {
    if (!bh1750_inited) {
        return;
    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    bh1750_inited = false;
}

// read most recent result
esp_err_t bh1750_start_measure(bh1750_measure_mode_t mode) {
    esp_err_t err = bh1750_init();
    if (err != ESP_OK) {
        return err;
    }
    return bh1750_i2c_write_cmd(mode);

}

esp_err_t bh1750_read_lux(float *lux) {
    esp_err_t err = bh1750_init();
    if (err != ESP_OK) {
        return err;
    }

    uint16_t read_raw;
    err = bh1750_i2c_read(&read_raw);
    if (err != ESP_OK) {
        return err;
    }

    *lux = read_raw / 1.2f;
    return ESP_OK;
}

esp_err_t bh1750_reset() {
    esp_err_t err = bh1750_init();
    if (err != ESP_OK) {
        return err;
    }
    return bh1750_i2c_write_cmd(BH1750_CMD_RESET);
}

esp_err_t bh1750_poweron() {
    esp_err_t err = bh1750_init();
    if (err != ESP_OK) {
        return err;
    }
    return bh1750_i2c_write_cmd(BH1750_CMD_POWER_ON);
}

esp_err_t bh1750_powerdown() {
    esp_err_t err = bh1750_init();
    if (err != ESP_OK) {
        return err;
    }
    return bh1750_i2c_write_cmd(BH1750_CMD_POWER_DOWN);
}
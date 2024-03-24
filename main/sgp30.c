//
// Created by yang on 2024/3/3.
//

#include "sgp30.h"

#include <stdio.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/i2c_master.h"

#include "common_utils.h"

#define I2C_MASTER_TIMEOUT_MS       200
#define SGP30_FEATURESET 0x0020

#define SGP30_ADDR                 0x58

#define SGP30_RESET 0x0006
//type:2ms  max:10ms
#define SGP30_IAQ_INIT 0x2003
// 10ms 12ms response len: 6
#define SGP30_MEASURE_IAQ 0x2008
// 1ms 10ms
#define SGP30_GET_IAQ_BASELINE 0x2015
// 1ms 10ms
#define SGP30_SET_IAQ_BASELINE 0x201E
// 1ms 10ms
#define SGP30_SET_ABSOLUTE_HUMIDITY 0x2061
// 200ms
#define SGP30_MEASURE_TEST 0x2032
// 1ms 10ms
#define SGP30_GET_FEATURE_SET 0x202f
// 20ms 25ms
#define SGP30_MEASURE_RAW 0x2050
// 1ms 10ms
#define SGP30_GET_TVOC_INCEPTIVE_BASELINE 0x20b3
// 1ms 10ms
#define SGP30_SET_TVOC_BASELINE 0x2077

#define SGP30_GET_SERIAL_ID 0x3682

ESP_EVENT_DEFINE_BASE(BIKE_GASS_SENSOR_EVENT);

static const char *TAG = "sgp30";
extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;
static bool sgp30_inited = false;

static uint16_t TVOC, CO2;
static uint16_t rawH2, rawEthanol;

static esp_err_t i2c_write_cmd(uint16_t reg_addr) {

    int ret;
    uint8_t write_buff[] = {reg_addr >> 8, reg_addr & 0xff};
    ret = i2c_master_transmit(dev_handle, write_buff, 2, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c cmd failed, for dev:%x addr:%x,  %s", SGP30_ADDR, reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t i2c_write_data(uint16_t reg_addr, uint8_t *data, size_t len) {
    int ret;
    uint8_t write_buff[2 + len];

    write_buff[0] = reg_addr >> 8;
    write_buff[1] = reg_addr & 0xff;
    memcpy(write_buff + 2, data, len);

    ret = i2c_master_transmit(dev_handle, write_buff, 2 + len, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c data failed, for dev:%x addr:%x,  %s", SGP30_ADDR, reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static uint8_t crc8(const uint8_t *data, uint8_t len) {
    const uint8_t POLYNOMIAL = 0x31;
    uint8_t crc = 0xFF;

    for (int j = len; j; --j) {
        crc ^= *data++;

        for (int i = 8; i; --i) {
            crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
        }
    }
    return crc;
}

static esp_err_t sgp_i2c_read(uint16_t *readdata, uint8_t read_len) {
    const uint8_t SHT40_WORD_LEN = 2;
    uint8_t replylen = read_len * (SHT40_WORD_LEN + 1);
    uint8_t replybuffer[replylen];

    esp_err_t err = i2c_master_receive(dev_handle,
                                       replybuffer, replylen, I2C_MASTER_TIMEOUT_MS);

    print_bytes(replybuffer, replylen);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for dev:%x %s", SGP30_ADDR, esp_err_to_name(err));
    }

    for (uint8_t i = 0; i < read_len; i++) {
        uint8_t crc = crc8(replybuffer + i * 3, 2);

        if (crc != replybuffer[i * 3 + 2]) {
            // return ESP_ERR_INVALID_CRC;
        }

        // success! store it
        readdata[i] = (replybuffer[i * 3] << 8) | replybuffer[i * 3 + 1];
    }

    ESP_LOGI(TAG, "read i2c  %d %s", err, esp_err_to_name(err));
    return err;
}

esp_err_t sgp30_read_serial_id() {
    i2c_write_cmd(SGP30_GET_SERIAL_ID);

    uint16_t reply[3];
    esp_err_t err = sgp_i2c_read(reply, 3);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "read serial fail %d %s", err, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "serial number %x %x %x", reply[0], reply[1], reply[2]);
    return ESP_OK;
}

void sgp30_init() {
    if (sgp30_inited) {
        return;
    }

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = SGP30_ADDR,
            .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

    sgp30_read_serial_id();

    ESP_LOGI(TAG, "inited...");
    sgp30_inited = true;
}

void spg30_deinit() {
    if (sgp30_inited) {
        i2c_master_bus_rm_device(dev_handle);
        sgp30_inited = false;
    }
}

esp_err_t sgp30_iaq_init() {
    return i2c_write_cmd(SGP30_IAQ_INIT);
}

esp_err_t sgp30_iaq_measure() {
    i2c_write_cmd(SGP30_MEASURE_IAQ);
    vTaskDelay(pdMS_TO_TICKS(30));

    uint16_t reply[2];
    esp_err_t err = sgp_i2c_read(reply, 2);
    if (err != ESP_OK) {
        return err;
    }
    TVOC = reply[1];
    CO2 = reply[0];
    ESP_LOGI(TAG, "get measure tvoc:%d co2:%d", TVOC, CO2);
    return ESP_OK;
}

esp_err_t sgp30_iaq_measure_raw() {
    i2c_write_cmd(SGP30_MEASURE_RAW);
    vTaskDelay(pdMS_TO_TICKS(25));

    uint16_t reply[2];
    esp_err_t err = sgp_i2c_read(reply, 2);
    if (err != ESP_OK) {
        return err;
    }
    rawEthanol = reply[1];
    rawH2 = reply[0];
    ESP_LOGI(TAG, "get measure raw rawEthanol:%d rawH2:%d", rawEthanol, rawH2);
    return ESP_OK;
}

esp_err_t sgp30_get_iaq_baseline(uint16_t *co2_base, uint16_t *tvoc_base) {
    i2c_write_cmd(SGP30_GET_SERIAL_ID);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint16_t reply[2];
    esp_err_t err = sgp_i2c_read(reply, 2);
    if (err != ESP_OK) {
        return err;
    }

    *co2_base = reply[0];
    *tvoc_base = reply[1];
    return ESP_OK;
}

esp_err_t sgp30_set_iaq_baseline(uint16_t co2_base, uint16_t tvoc_base) {
    uint8_t buff[6];
    buff[0] = tvoc_base >> 8;
    buff[1] = tvoc_base & 0xFF;
    buff[2] = crc8(buff, 2);
    buff[3] = co2_base >> 8;
    buff[4] = co2_base & 0xFF;
    buff[5] = crc8(buff + 3, 2);

    esp_err_t err = i2c_write_data(SGP30_SET_IAQ_BASELINE, buff, 6);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t sgp30_set_humidity(float temp, float hum) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absolute_humidity =
            216.7f * ((hum / 100.0f) * 6.112f * exp((17.62f * temp) / (243.12f + temp)) / (273.15f + temp)); // [g/m^3]
    const uint32_t absolute_humidity_scaled = (uint32_t) (1000.0f * absolute_humidity); // [mg/m^3]
    if (absolute_humidity_scaled > 256000) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t ah_scaled =
            (uint16_t) (((uint64_t) absolute_humidity_scaled * 256 * 16777) >> 24);

    uint8_t buff[3];
    buff[0] = ah_scaled >> 8;
    buff[1] = ah_scaled & 0xFF;
    buff[2] = crc8(buff, 2);

    esp_err_t err = i2c_write_data(SGP30_SET_ABSOLUTE_HUMIDITY, buff, 3);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t sgp30_get_result(uint16_t *tvoc, uint16_t *co2) {
    *tvoc = TVOC;
    *co2 = CO2;
    return ESP_OK;
}

esp_err_t sgp30_reset() {
    // use common reset command
    return i2c_write_cmd(SGP30_RESET);
}


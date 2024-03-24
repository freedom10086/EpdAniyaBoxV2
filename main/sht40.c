#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/i2c_master.h"

#include "common_utils.h"
#include "sht40.h"

#define I2C_MASTER_TIMEOUT_MS       100
#define SHT_USE_LST_RESULT_TIMEOUT_MS 1000

#define SHT40_ADDR                 0x44

#define SHT40_MEAS_HIGHREP          0xFD     /* measure T & RH with high precision (high repeatability) */
#define SHT40_MEAS_MEDREP           0xF6     /* measure T & RH with medium precision (medium repeatability) */
#define SHT40_MEAS_LOWREP           0xE0     /* measure T & RH with lowest precision (low repeatability) */

#define SHT40_SERIAL_NUMBER         0x89
#define SHT40_SOFT_RESET            0x94

#define SHT40_MEAS_HIGHREP_HEATER_1            0x39    /* activate heater with 200mW for 1s, including a high precision measurement */
#define SHT40_MEAS_HIGHREP_HEATER_2            0x32    /* activate heater with 200mW for 0.1s including a high precision measurement */
#define SHT40_MEAS_HIGHREP_HEATER_3            0x2f    /* activate heater with 110mW for 1s including a high precision measurement */
#define SHT40_MEAS_HIGHREP_HEATER_4            0x24    /* activate heater with 110mW for 0.1s including a high precision measurement */
#define SHT40_MEAS_HIGHREP_HEATER_5            0x1e    /* activate heater with 20mW for 1s including a high precision measurement */
#define SHT40_MEAS_HIGHREP_HEATER_6            0x15    /* activate heater with 20mW for 0.1s including a high precision measurement */

/**
 *
 * Measurement duration
 *                          Typ.    Max.
 * Low repeatability        1.3     1.7ms
 * Medium repeatability     3.7     4.5ms
 * High repeatability       6.9     8.2ms
 */

ESP_EVENT_DEFINE_BASE(BIKE_TEMP_HUM_SENSOR_EVENT);

static const char *TAG = "sht40";
extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;
static bool sht40_inited = false;

static sht_data_t sht_data;

static float _lst_temp, _lst_hum;
static uint32_t _lst_read_tick = 0;

static sht_accuracy_t _lst_start_measure_accuracy = SHT_SENSOR_ACCURACY_MEDIUM;
static uint32_t _lst_start_measure_tick = 0;

RTC_DATA_ATTR bool _sht40_device_id_checked = false;

static TaskHandle_t _sht_measure_task_hdl = NULL;

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

static esp_err_t sht_i2c_write_cmd(uint8_t cmd) {
    int ret;
    uint8_t u8_w_buf[] = {cmd};
    ret = i2c_master_transmit(dev_handle, u8_w_buf, 1, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c cmd failed, for cmd %x,  %s", cmd, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t sht_i2c_read(uint16_t *readdata, uint8_t read_len) {
    const uint8_t SHT40_WORD_LEN = 2;
    uint8_t replylen = read_len * (SHT40_WORD_LEN + 1);
    uint8_t replybuffer[replylen];

    esp_err_t err = i2c_master_receive(dev_handle,
                                       replybuffer, replylen, I2C_MASTER_TIMEOUT_MS);

    print_bytes(replybuffer, replylen);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for dev:%x %s", SHT40_ADDR, esp_err_to_name(err));
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

void sht40_reset() {
    ESP_LOGI(TAG, "Reset Sht40...");
    sht_i2c_write_cmd(SHT40_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(5));
}

void sht40_init() {
    if (sht40_inited) {
        return;
    }

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = SHT40_ADDR,
            .scl_speed_hz = 200000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

    if (!_sht40_device_id_checked) {
        uint8_t failed_cnt = 0;
        uint16_t serial_number[2];
        esp_err_t err;

        sht40_start:
        err = sht_i2c_write_cmd(SHT40_SERIAL_NUMBER);
        if (err == ESP_OK) {
            err = sht_i2c_read(serial_number, 2);
        }
        //err = sht_i2c_write_read(SHT40_SERIAL_NUMBER, serial_number, 2);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read status crc check failed! err:%s", esp_err_to_name(err));
            failed_cnt++;
            if (failed_cnt >= 3) {
                ESP_LOGE(TAG, "read sht40 status failed for 3 times...");
                sht40_reset();
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(3));
            goto sht40_start;
        } else {
            ESP_LOGI(TAG, "sht40 serial num %X %X", serial_number[0], serial_number[1]);
        }
        _sht40_device_id_checked = true;
    }
    sht40_inited = true;
    ESP_LOGI(TAG, "inited");
}

void sht40_deinit() {
    if (_sht_measure_task_hdl != NULL) {
        vTaskDelete(_sht_measure_task_hdl);
        _sht_measure_task_hdl = NULL;
    }

    if (sht40_inited) {
        i2c_master_bus_rm_device(dev_handle);
        sht40_inited = false;
    }
}

esp_err_t sht40_start_measure(sht_accuracy_t accuracy) {
    sht40_init();

    esp_err_t err = ESP_ERR_INVALID_ARG;
    switch (accuracy) {
        case SHT_SENSOR_ACCURACY_LOW:
            err = sht_i2c_write_cmd(SHT40_MEAS_LOWREP);
            break;
        case SHT_SENSOR_ACCURACY_HIGH:
            err = sht_i2c_write_cmd(SHT40_MEAS_HIGHREP);
            break;
        default:
            err = sht_i2c_write_cmd(SHT40_MEAS_MEDREP);
            break;
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "start measure for accuracy: %d", accuracy);
        _lst_start_measure_tick = xTaskGetTickCount();
        _lst_start_measure_accuracy = accuracy;
    }

    return err;
}

esp_err_t sht40_get_temp_hum(float *temp, float *hum) {
    uint32_t curr_tick = xTaskGetTickCount();
    if (_lst_read_tick > 0 && pdTICKS_TO_MS(curr_tick - _lst_read_tick) < SHT_USE_LST_RESULT_TIMEOUT_MS) {
        *temp = _lst_temp;
        *hum = _lst_hum;
        return ESP_OK;
    }

    if (_lst_start_measure_tick == 0) {
        // not start before
        sht40_start_measure(_lst_start_measure_accuracy);
        curr_tick = xTaskGetTickCount();
    }

    int32_t delay_ms = _lst_start_measure_accuracy - pdTICKS_TO_MS(curr_tick - _lst_start_measure_tick);
    if (delay_ms > 0) {
        ESP_LOGE(TAG, "sleep ms %ld", delay_ms);
        vTaskDelay(max(1, delay_ms));
    }

    uint16_t read[2];
    esp_err_t err = sht_i2c_read(read, 2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "======================= %d %s", err, esp_err_to_name(err));
        _lst_start_measure_tick = 0;
        return err;
    }

    _lst_start_measure_tick = 0;

    *temp = -45 + 175 * (read[0] * 1.0f / 65535);
    *hum = -6 + 125 * (read[1] * 1.0f / 65535);
    if (*hum > 100)
        *hum = 100;
    if (*hum < 0)
        *hum = 0;

    _lst_temp = *temp;
    _lst_hum = *hum;
    _lst_read_tick = xTaskGetTickCount();

    return err;
}

static void sht_task_entry(void *arg) {
    sht_accuracy_t *accuracy = arg;
    esp_err_t err;
    while (1) {
        err = sht40_start_measure(*accuracy);
        if (err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        err = sht40_get_temp_hum(&sht_data.temp, &sht_data.hum);
        if (err == ESP_OK) {
            common_post_event_data(BIKE_TEMP_HUM_SENSOR_EVENT, SHT_SENSOR_UPDATE, &sht_data, sizeof(sht_data_t));
        } else {
            common_post_event(BIKE_TEMP_HUM_SENSOR_EVENT, SHT_SENSOR_READ_FAILED);
        };
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

esp_err_t sht40_start_continues_measure(sht_accuracy_t accuracy) {
    if (_sht_measure_task_hdl != NULL) {
        return ESP_OK;
    }

    sht40_init();

    /* Create key click detect task */
    BaseType_t err = xTaskCreate(
            sht_task_entry,
            "sht_task",
            3072,
            &accuracy,
            5,
            &_sht_measure_task_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create sht measure task failed");
    }
    ESP_LOGI(TAG, "sht measure task create success!");
    return ESP_OK;
}

void sht40_stop_continues_measure() {
    if (_sht_measure_task_hdl != NULL) {
        vTaskDelete(_sht_measure_task_hdl);
        _sht_measure_task_hdl = NULL;
    }
}

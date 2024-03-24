//
// Created by yang on 2024/3/3.
//
#include <math.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <string.h>
#include "driver/i2c_master.h"

#include "spl06.h"

ESP_EVENT_DEFINE_BASE(BIKE_PRESSURE_SENSOR_EVENT);

// or 0x76 if SDO is low
// 0x77 if SDO is high
#define SPL06_ADDR 0x77

#define PSR_B2 0x00
#define PSR_B1 0x01
#define PSR_B0 0x02

#define TMP_B2 0x03
#define TMP_B1 0x04
#define TMP_B0 0x05

/**
 * 每秒采样次数  -- 只在background模式生效
 * bit [6:4] PM_RATE rw
 *      000 1/s 100 16
 *      001 2/s 101 32
 *      010 4   110 64
 *      011 8   111 128
 *
 * 超采样次数
 * bit [3:0] PM_PRC rw
 *      0000 1  0100 16   -- bit shift. CFG_REG
 *      0001 2  0101 32     -- 53.2ms
 *      0010 4  0110 64     -- 104.4ms
 *      0011 8  0111 128    -- 206.8ms
 */
#define PRS_CFG 0x06

/**
 * bit 7 TMP_ EXT rw  -- be 1
 * bit [6:4] TMP_RATE rw
 * bit [2:0] TM_PRC rw
 *      000 1       100 16        1次采样 3.6ms
 *      001 2       101 32
 *      010 4       110 64
 *      011 8       111 128
 */
#define TMP_CFG 0x07

/**
 * bit 7 COEF_RDY   1- ready
 * bit 6 SENSOR_RDY 1 - Sensor initialization complete
 * bit 5 TMP_RDY 1 - New temperature measurement is ready. 如果开启了fifo此位失效
 * bit 4 PRS_ RDY 1 - New pressure measurement is ready.  如果开启了fifo此位失效
 * bit [2:0] MEAS_CRTL rw
 *      000 - Idle / Stop background measurement
 *      001 - Pressure measurement
 *      010 - Temperature measurement
 *
 *      Background Mode:
 *      101 - Continuous pressure measurement
 *      110 - Continuous temperature measurement
 *      111 - Continuous pressure and temperature measurement
 */
#define MEAS_CFG 0x08

/**
 * bit 7 INT_ HL rw
 * bit [6:4] INT_ SEL rw
 * bit 3 TMP_ SHIFT_ EN (rw)  Temperature result bit-shift, Note: Must be set to '1' when the oversampling rate is >8 times.
 * bit 2 PRS_ SHIFT_ EN (rw) Pressure result bit-shift, Must be set to '1' when the oversampling rate is >8 times.
 * bit 1 FIFO_ EN (rw)  1 - Enable the FIFO
 */
#define CFG_REG 0x09

/**
 * bit 2 INT_ FIFO_ FULL
 * bit 1 INT_ TMP
 * bit 0 INT_ PRS
 */
#define INT_STS 0x0A

/**
 * bit 1 FIFO_ FULL   未开启fifo无效
 * bit 0 FIFO_ EMPTY 未开启fifo无效
 */
#define FIFO_STS 0x0B

/**
 * bit 7 FIFO_ FLUSH (w)    1-Empty FIFO After reading out all data from the FIFO, write '1' to clear all old data.
 * bit [3:0] SOFT_RST [3:0] (w)  1001 for reset
 */
#define RESET 0x0C

/**
 * [7:4] PROD_ID
 * [3:0] REV_ID
 */
#define SPL06_ID  0x0D

/**
 * from 0x10 to 0x21
 */
#define COEF 0x10

#define I2C_MASTER_TIMEOUT_MS 100

#define TAG "spl06"

typedef struct {
    bool en_fifo;

    // TMP_PRC[2:0]        : 0      | 1 | 2 | 3 | 4  | 5  | 6  | 7
    // oversampling (times): single | 2 | 4 | 8 | 16 | 32 | 64 | 128
    uint8_t oversampling_t;

    // PM_PRC[3:0]         : 0      | 1   | 2   | 3    | 4    | 5    | 6     | 7
    // oversampling (times): single | 2   | 4   | 8    | 16   | 32   | 64    | 128
    uint8_t oversampling_p;

    int32_t raw_temp;

    bool raw_temp_valid;

    int32_t raw_pressure;

    // measurement mode and type
    /**
     * Standby Mode
        000 - Idle / Stop background measurement
       Command Mode
        001 - Pressure measurement
        010 - Temperature measurement
        011 - na.
        100 - na.
       Background Mode
        101 - Continuous pressure measurement
        110 - Continuous temperature measurement
        111 - Continuous pressure and temperature measurement
     */
    uint8_t meas_ctrl;

    bool coef_ready;

    bool sensor_ready;

    bool temp_ready;

    bool pressure_ready;

    bool fifo_full;

    bool fifo_empty;

    int32_t fifo[32];

    uint8_t fifo_len;
} spl06_t;

static spl06_t spl06;
static TaskHandle_t spl06_task_hdl = NULL;

extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;
static spl06_event_data_t event_data;

static bool spl06_inited = false;
RTC_DATA_ATTR static bool coef_load = false;

RTC_DATA_ATTR static int16_t c0 = 0;
RTC_DATA_ATTR static int16_t c1 = 0;
RTC_DATA_ATTR static int32_t c00 = 0;
RTC_DATA_ATTR static int32_t c10 = 0;
RTC_DATA_ATTR static int16_t c01 = 0;
RTC_DATA_ATTR static int16_t c11 = 0;
RTC_DATA_ATTR static int16_t c20 = 0;
RTC_DATA_ATTR static int16_t c21 = 0;
RTC_DATA_ATTR static int16_t c30 = 0;

static void spl06_task_entry(void *arg);

static float calc_altitude(float pressure) {
    // calc height
    const float T1 = 15.0f + 273.15f;       // temperature at base height in Kelvin
    const float a = -6.5f / 1000.0f;       // temperature gradient in degrees per metre
    const float g = 9.80665f;              // gravity constant in m/s/s
    const float R = 287.05f;               // ideal gas constant in J/kg/K
    float pK = pressure / SEA_LEVEL_PRESSURE;
    float altitude = (((powf(pK, (-(a * R) / g))) * T1) - T1) / a;
    return altitude;
}

static float calc_altitude_v2(float pressure) {
    float altitude = 44330 * (1 - pow(pressure / SEA_LEVEL_PRESSURE, 1 / 5.225));
    return altitude;
}

static esp_err_t i2c_read(uint8_t reg_addr, uint8_t *data, size_t len) {
    esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read i2c failed, for dev:%x addr:%x,  %s", SPL06_ADDR, reg_addr, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t i2c_write_data(uint8_t reg_addr, uint8_t data) {
    uint8_t write_buf[2] = {reg_addr, data};
    esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write i2c cmd failed, for addr %x,  %s", reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

static uint8_t oversampling_rate_to_state(uint8_t rate) {
    switch (rate) {
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        case 16:
            return 4;
        case 32:
            return 5;
        case 64:
            return 6;
        case 128:
            return 7;
        default:
            return 0;
    }
}

static float scale_factor(int oversampling_rate) {
    float k;
    switch (oversampling_rate) {
        case 1: // 0
            k = 524288.0f;
            break;

        case 2: // 1
            k = 1572864.0f;
            break;

        case 4: // 2
            k = 3670016.0f;
            break;

        case 8: // 3
            k = 7864320.0f;
            break;

        case 16: // 4
            k = 253952.0f;
            break;

        case 32: // 5
            k = 516096.0f;
            break;

        case 64: // 6
            k = 1040384.0f;
            break;

        case 128: // 7
            k = 2088960.0f;
            break;
        default:
            ESP_LOGE(TAG, "spl06 unknown oversampling_rate %d", oversampling_rate);
            return -1;
    }

    return k;
}

esp_err_t spl06_init() {
    if (spl06_inited) {
        return ESP_OK;
    }

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = SPL06_ADDR,
            .scl_speed_hz = 200000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

    // check device id
    uint8_t device_id;
    for (int i = 0; i < 3; ++i) {
        i2c_read(SPL06_ID, &device_id, 1);
        if (device_id == 0x10) {
            ESP_LOGI(TAG, "PROD_ID:%x, REV_ID:%x", device_id >> 4, device_id & 0x0f);
            break;
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    if (device_id != 0x10) {
        // not spl06 break
        ESP_LOGE(TAG, "not spl06 device_id: %x", device_id);
        common_post_event(BIKE_PRESSURE_SENSOR_EVENT, SPL06_SENSOR_INIT_FAILED);
        return ESP_ERR_INVALID_STATE;
    }

    // todo set outer
    spl06.oversampling_t = 16;
    spl06.oversampling_p = 64;
    spl06.raw_temp_valid = 0;

    ESP_LOGI(TAG, "inited successful...");
    spl06_inited = true;
    return ESP_OK;
}

void spl06_deinit() {
    if (spl06_inited) {
        spl06_stop();
        i2c_master_bus_rm_device(dev_handle);

        spl06_inited = false;
    }
}

void spl06_reset() {
    i2c_write_data(RESET, 0b10001001);
    ESP_LOGI(TAG, "reset...");
    vTaskDelay(pdMS_TO_TICKS(5));
    spl06.fifo_len = 0;
}

esp_err_t spl06_start(bool en_fifo) {
    if (spl06_task_hdl != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    spl06.en_fifo = en_fifo;

    i2c_write_data(PRS_CFG,
                   2 << 4 | oversampling_rate_to_state(
                           spl06.oversampling_p));    // Pressure 4 measure per second, 64x oversampling, 104ms - 420ms
    i2c_write_data(TMP_CFG, 1 << 7 | 2 << 4 |
                            oversampling_rate_to_state(
                                    spl06.oversampling_t));    // Temp 4 measure per second, 16x oversampling, 57.6ms - 230ms

    i2c_write_data(CFG_REG, (spl06.oversampling_t > 8 ? 1 : 0) << 3 // temp shift
                            | (spl06.oversampling_p > 8 ? 1 : 0) << 2 // pressure shift
                            | (spl06.en_fifo ? 1 : 0) << 1); // en fifo

    i2c_write_data(MEAS_CFG, 0b0111); // background continuous temp and pressure measurement

    // clear fifo
    // i2c_write_data(RESET, 0x80);

    /* Create task */
    BaseType_t err = xTaskCreate(
            spl06_task_entry,
            "spl06_tsk",
            2048,
            NULL,
            5,
            &spl06_task_hdl);

    ESP_LOGI(TAG, "create spl06 task success");
    return err;
}

void spl06_stop() {
    if (!spl06_inited) {
        return;
    }

    if (spl06_task_hdl != NULL) {
        vTaskDelete(spl06_task_hdl);
        spl06_task_hdl = NULL;
    }

    spl06.meas_ctrl = spl06.meas_ctrl & 0b11111000;
    i2c_write_data(MEAS_CFG, spl06.meas_ctrl);
}

void spl06_fifo_state() {
    uint8_t fifo_state = 0;

    // check fifo en
    i2c_read(CFG_REG, &fifo_state, 1);
    spl06.en_fifo = (fifo_state >> 1) & 0x01;
    // ESP_LOGI(TAG, "config state %x", fifo_state);

    if (spl06.en_fifo) {
        fifo_state = 0;
        i2c_read(FIFO_STS, &fifo_state, 1);

        spl06.fifo_empty = fifo_state & 0x01;
        spl06.fifo_full = (fifo_state >> 1) & 0x01;

        ESP_LOGI(TAG, "fifo state %x", fifo_state);
    } else {
        spl06.fifo_empty = false;
        spl06.fifo_full = false;
    }
}

void spl06_meassure_state() {
    uint8_t state = 0;
    i2c_read(MEAS_CFG, &state, 1);

    spl06.coef_ready = (state >> 7) & 0x01;
    spl06.sensor_ready = (state >> 6) & 0x01;
    spl06.temp_ready = (state >> 5) & 0x01;
    spl06.pressure_ready = (state >> 4) & 0x01;
    spl06.meas_ctrl = state & 0x07;

    ESP_LOGI(TAG, "measure state %x pressure_ready:%d temp_ready:%d", state, spl06.pressure_ready, spl06.temp_ready);
}

/**
 * 最后一位
 * '1' if the result is a pressure measurement.
 * '0' if it is a temperature measurement.
 */
void spl06_read_raw_fifo() {
    spl06.fifo_len = 0;
    while (1) {
        uint8_t buf[3] = {0};
        i2c_read(PSR_B2, buf, 1);
        i2c_read(PSR_B1, &buf[1], 1);
        i2c_read(PSR_B0, &buf[2], 1);
        uint32_t fifo_raw = buf[0] << 16 | buf[1] << 8 | buf[2];
        if (fifo_raw == 0x800000) {
            // no data
            return;
        }
        if (fifo_raw & 0x800000) {
            fifo_raw = 0xFF000000 | fifo_raw;
        }

        spl06.fifo[spl06.fifo_len] = fifo_raw;
        spl06.fifo_len = spl06.fifo_len + 1;
        if (spl06.fifo_len >= 32) {
            return;
        }
    }
}

uint32_t spl06_read_raw_temp() {
    uint8_t buf[3] = {0};
    i2c_read(TMP_B2, &buf[0], 1);
    i2c_read(TMP_B1, &buf[1], 1);
    i2c_read(TMP_B0, &buf[2], 1);

    uint32_t temp_raw = buf[0] << 16 | buf[1] << 8 | buf[2];
    if (temp_raw & 0x800000) {
        temp_raw = 0xFF000000 | temp_raw;
    }
    spl06.raw_temp = temp_raw;
    spl06.raw_temp_valid = true;
    return temp_raw;
}

uint32_t spl06_read_raw_pressure() {
    uint8_t buf[3] = {0};
    i2c_read(PSR_B2, &buf[0], 1);
    i2c_read(PSR_B1, &buf[1], 1);
    i2c_read(PSR_B0, &buf[2], 1);

    int32_t pressure_raw = buf[0] << 16 | buf[1] << 8 | buf[2];
    if (pressure_raw & 0x800000) {
        pressure_raw = 0xFF000000 | pressure_raw;
    }

    spl06.raw_pressure = pressure_raw;
    return pressure_raw;
}

float spl06_get_temperature() {
    float fTsc = (float) spl06.raw_temp / scale_factor(spl06.oversampling_t);
    float temp = (float) c0 * 0.5f + (float) c1 * fTsc;
    return temp;
}

float spl06_get_pressure() {
    float fPsc = (float) spl06.raw_pressure / scale_factor(spl06.oversampling_p);
    float fTsc = (float) spl06.raw_temp / scale_factor(spl06.oversampling_t);

    float qua2 = (float) c10 + fPsc * ((float) c20 + fPsc * (float) c30);
    float qua3 = fTsc * fPsc * ((float) c11 + fPsc * (float) c21);

    float pressure = (float) c00 + fPsc * qua2 + fTsc * (float) c01 + qua3;
    return pressure;
}

static void read_coef_data() {
    if (coef_load) {
        return;
    }

    // wait for COEF READY
    while (1) {
        spl06_meassure_state();
        if (spl06.coef_ready) {
            ESP_LOGI(TAG, "spl06 coef ready...");
            break;
        }

        ESP_LOGI(TAG, "spl06 wait for coef ready...");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // load COEF from 0x10 - 0x21
    for (int i = COEF; i <= 0x21; ++i) {
        uint8_t cofe_buf = 0;
        i2c_read(i, &cofe_buf, 1);
        if (i == 0x10) {
            c0 = (cofe_buf << 4);
        } else if (i == 0x11) {
            c0 = c0 | (cofe_buf >> 4);
            if (c0 & 0x800) {
                c0 = 0xF000 | c0;
            }
            c1 = ((cofe_buf & 0x0f) << 8);
        } else if (i == 0x12) {
            c1 = c1 | cofe_buf;
            if (c1 & 0x800) {
                c1 = 0xF000 | c1;
            }
        } else if (i == 0x13) {
            c00 = (cofe_buf << 12);
        } else if (i == 0x14) {
            c00 = c00 | (cofe_buf << 4);
        } else if (i == 0x15) {
            c00 = c00 | (cofe_buf >> 4);
            if (c00 & 0x080000) {
                c00 = 0xFFF00000 | c00;
            }
            c10 = (cofe_buf & 0x0f) << 16;
        } else if (i == 0x16) {
            c10 = c10 | (cofe_buf << 8);
        } else if (i == 0x17) {
            c10 = c10 | cofe_buf;
            if (c10 & 0x080000) {
                c10 = 0xFFF00000 | c10;
            }
        } else if (i == 0x18) {
            c01 = cofe_buf << 8;
        } else if (i == 0x19) {
            c01 = c01 | cofe_buf;
        } else if (i == 0x1a) {
            c11 = cofe_buf << 8;
        } else if (i == 0x1b) {
            c11 = c11 | cofe_buf;
        } else if (i == 0x1c) {
            c20 = cofe_buf << 8;
        } else if (i == 0x1d) {
            c20 = c20 | cofe_buf;
        } else if (i == 0x1e) {
            c21 = cofe_buf << 8;
        } else if (i == 0x1f) {
            c21 = c21 | cofe_buf;
        } else if (i == 0x20) {
            c30 = cofe_buf << 8;
        } else if (i == 0x21) {
            c30 = c30 | cofe_buf;
        }
    }

    ESP_LOGI(TAG, "spl06 read COEF c0:%x c1:%x c00:%lx c10:%lx c01:%x c11:%x c20:%x c21:%x c30:%x",
             c0, c1, c00, c10, c01, c11, c20, c21, c30);

    coef_load = true;
}

static void spl06_task_entry(void *arg) {
    // spl06_reset();
    // vTaskDelay(pdMS_TO_TICKS(30));

    read_coef_data();

    // wait for sensor ready
    while (1) {
        if (spl06.sensor_ready) {
            ESP_LOGI(TAG, "spl06 sensor ready...");
            break;
        }
        spl06_meassure_state();
        ESP_LOGI(TAG, "spl06 wait for sensor ready...");
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    while (1) {
        spl06_meassure_state();
        spl06_fifo_state();
        vTaskDelay(pdMS_TO_TICKS(320));

        if (spl06.en_fifo) {
            // read from fifo
            if (spl06.fifo_full) {
                spl06_read_raw_fifo();
                ESP_LOGI(TAG, "spl06 read fifo %d", spl06.fifo_len);
                for (uint8_t i = 0; i < spl06.fifo_len; i++) {
                    bool is_pressure = spl06.fifo[i] & 0x01;
                    int32_t item = spl06.fifo[i]; // >> 1;
                    if (is_pressure) {
                        spl06.raw_pressure = item;
                        ESP_LOGI(TAG, "spl06 fifo pressure data %d %f, ", i, spl06_get_pressure(spl06));
                    } else {
                        spl06.raw_temp = item;
                        ESP_LOGI(TAG, "spl06 fifo temp data %d %f, ", i, spl06_get_temperature(spl06));
                    }
                }
            }
        } else {
            bool data_updated = false;
            if (spl06.pressure_ready && spl06.raw_temp_valid) {
                spl06_read_raw_pressure();
                float pressure = spl06_get_pressure();
                ESP_LOGI(TAG, "spl06 raw_pressure %ld,  pressure: %f altitude:%f altitudeV2:%f",
                         spl06.raw_pressure, pressure, calc_altitude(pressure), calc_altitude_v2(pressure));

                event_data.pressure = pressure;
                event_data.altitude = calc_altitude(pressure);

                data_updated = true;
            }

            if (spl06.temp_ready) {
                spl06_read_raw_temp();
                float temp = spl06_get_temperature();
                ESP_LOGI(TAG, "spl06 raw_temp %ld,  temp: %f", spl06.raw_temp, temp);

                event_data.temp = temp;
                data_updated = true;
            }

            if (data_updated) {
                common_post_event_data(BIKE_PRESSURE_SENSOR_EVENT, SPL06_SENSOR_UPDATE, &event_data,
                                       sizeof(spl06_event_data_t));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
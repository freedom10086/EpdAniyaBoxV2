//
// Created by yang on 2024/1/10.
//

#include "esp_log.h"
#include "setting.h"

#include "rx8025t.h"

#define TAG "BOX_SETTING"

static uint8_t ping = 0;

/**
 *  0-6
 * [year month day week hour minute second]
 */
esp_err_t box_setting_load(uint8_t cmd, uint8_t *out, uint16_t *out_len) {
    esp_err_t err;
    uint16_t len = 0;

    box_setting_time_t t;
    err = rx8025t_get_time(&t.year, &t.month, &t.day, &t.week, &t.hour, &t.minute, &t.second);
    if (err != ESP_OK) {
        return err;
    }

    out[0] = t.year;
    out[1] = t.month;
    out[2] = t.day;
    out[3] = t.week;
    out[4] = t.hour;
    out[5] = t.minute;
    out[6] = t.second;
    len += 7;

    *out_len = len;
    return err;
}

esp_err_t box_setting_apply(uint8_t cmd, uint8_t *data, uint16_t data_len) {
    esp_err_t err;
    switch (cmd) {
        case BOX_SETTING_CMD_SET_TIME:
            if (data_len != 7) {
                ESP_LOGW(TAG, "BOX_SETTING_CMD_SET_TIME data len should be 7 but %d", data_len);
                return ESP_ERR_INVALID_ARG;
            }
            err = rx8025t_set_time(data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
            ESP_LOGI(TAG, "set time %d %d %d %d %d %d %d, res:%d", data[0], data[1], data[2], data[3], data[4], data[5],
                     data[6], err);
            if (err != ESP_OK) {
                return err;
            }
            return ESP_OK;
        default:
            ESP_LOGW(TAG, "un supported cmd:%d len:%d", cmd, data_len);
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
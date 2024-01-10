//
// Created by yang on 2024/1/10.
//

#ifndef HELLO_WORLD_SETTING_H
#define HELLO_WORLD_SETTING_H

#include "stdio.h"
#include "common_utils.h"

#define BOX_SETTING_CMD_PING  0
#define BOX_SETTING_CMD_LOAD_CONFIG 1

#define BOX_SETTING_CMD_SET_TIME 2

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} box_setting_time_t;

esp_err_t box_setting_load(uint8_t cmd, uint8_t *out, uint16_t *out_len);

esp_err_t box_setting_apply(uint8_t cmd, uint8_t *data, uint16_t data_len);

#endif //HELLO_WORLD_SETTING_H

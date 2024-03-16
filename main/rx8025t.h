//
// Created by yang on 2023/12/18.
//

#ifndef HELLO_WORLD_RX8025T_H
#define HELLO_WORLD_RX8025T_H

#include <esp_event_base.h>
#include "esp_types.h"
#include "esp_event.h"
#include "time.h"

#include "event_common.h"
#include "common_utils.h"

ESP_EVENT_DECLARE_BASE(BIKE_DATE_TIME_SENSOR_EVENT);

// default high level when intr happens cone to low
#define RX8025_INT_GPIO_NUM 3

//将X的第Y位置1
#define setbit(x, y) x|=(1<<y)
//将X的第Y位清0
#define clrbit(x, y) x&=~(1<<y)

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rx8025_time_t;

typedef struct {
    uint8_t en;
    uint8_t mode; /* 1 day mode 0 week mode */
    uint8_t af;
    uint8_t minute;
    uint8_t hour;
    uint8_t day_week;
} rx8025t_alarm_t;

typedef enum {
    RX8025T_SENSOR_INIT_FAILED = 0,
    RX8025T_SENSOR_UPDATE,
    RX8025T_ALARM_CONFIG_UPDATE,
    RX8025T_SENSOR_READ_FAILED,
    RX8025T_SENSOR_ALARM_INTR,
    RX8025T_SENSOR_FIX_TIME_INTR,
} rx8025t_event_id_t;

typedef struct {
    uint8_t data;
} rx8025t_data_t;

esp_err_t rx8025t_init();

esp_err_t rx8025t_deinit();

// year 0-99
// month 1-12
// day 1 - 31
// week bit0 Sunday  bt6 Saturday 0-6 Sunday -> Saturday
// hour 0-23 minute 0-59 second 0-59
esp_err_t
rx8025t_set_time(uint8_t year, uint8_t month, uint8_t day, uint8_t week, uint8_t hour, uint8_t minute, uint8_t second);

esp_err_t rx8025t_get_time(uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *week, uint8_t *hour, uint8_t *minute,
                           uint8_t *second);

esp_err_t rx8025t_get_time_ts(time_t *ts);

// per_second true: second mode, false: minute mode
esp_err_t rx8025t_set_update_time_intr(uint8_t en, uint8_t per_minute);

esp_err_t rx8025t_read_update_time_intr(uint8_t *en, uint8_t *per_minute, uint8_t *flag);

esp_err_t rx8025t_clear_update_time_intr_flag();

// max 60 * 4095 second
esp_err_t rx8025t_set_fixed_time_intr(uint8_t en, uint8_t intr_en, uint16_t total_ts);

esp_err_t rx8025t_read_fixed_time_intr(uint8_t *en, uint8_t *intr_en, uint16_t *total_ts, uint8_t *flag);

esp_err_t rx8025t_clear_fixed_time_intr_flag();

// mode = 0 week mode, mode == 1 day mode
// af = alarm status
esp_err_t rx8025_load_alarm(rx8025t_alarm_t *alarm);

esp_err_t rx8025_set_alarm(rx8025t_alarm_t *alarm);

esp_err_t rx8025_set_alarm_en(uint8_t en);

esp_err_t rx8025_clear_alarm_flag();

esp_err_t rx8025_read_flags(uint8_t *uf, uint8_t *tf, uint8_t *af);

esp_err_t rx8025_read_alarm_flag(uint8_t *flag);

esp_err_t rx8025_clear_flags(uint8_t uf, uint8_t tf, uint8_t af);

esp_err_t rx8025_read_ctrl(uint8_t *uie, uint8_t *tie, uint8_t *aie);

#endif //HELLO_WORLD_RX8025T_H

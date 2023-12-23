//
// Created by yang on 2023/12/18.
//

#ifndef HELLO_WORLD_RX8025T_H
#define HELLO_WORLD_RX8025T_H

#include <esp_event_base.h>

ESP_EVENT_DECLARE_BASE(BIKE_DATE_TIME_SENSOR_EVENT);

// default high level when intr happens cone to low
#define RX8025_INT_GPIO_NUM 3

//将X的第Y位置1
#define setbit(x, y) x|=(1<<y)
//将X的第Y位清0
#define clrbit(x, y) x&=~(1<<y)

typedef enum {
    RX8025T_SENSOR_INIT_FAILED,
    RX8025T_SENSOR_UPDATE,
    RX8025T_SENSOR_READ_FAILED,
} rx8025t_event_id_t;

typedef struct {
    uint8_t data;
} rx8025t_data_t;

esp_err_t rx8025t_init();

esp_err_t rx8025t_deinit();

// year 0-99
// month 1-12
// day 1 - 31
// week bit0 Sunday  bt6 Saturday 1-7 monday -> sunday
// hour 0-23 minute 0-59 second 0-59
esp_err_t
rx8025t_set_time(uint8_t year, uint8_t month, uint8_t day, uint8_t week, uint8_t hour, uint8_t minute, uint8_t second);

esp_err_t rx8025t_get_time(uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *week, uint8_t *hour, uint8_t *minute,
                           uint8_t *second);

// per_second true: second mode, false: minute mode
esp_err_t rx8025t_set_update_time_intr(uint8_t en, uint8_t per_minute);

esp_err_t rx8025t_read_update_time_intr(uint8_t *en, uint8_t *per_minute, uint8_t *flag);

esp_err_t rx8025t_clear_update_time_intr_flag();

// max 60 * 4095 second
esp_err_t rx8025t_set_fixed_time_intr(uint8_t en, uint8_t intr_en, uint16_t total_ts);

esp_err_t rx8025t_read_fixed_time_intr(uint8_t* en, uint8_t* intr_en, uint16_t* total_ts, uint8_t *flag);

esp_err_t rx8025t_clear_fixed_time_intr_flag();

#endif //HELLO_WORLD_RX8025T_H

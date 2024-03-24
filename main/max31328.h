//
// Created by yang on 2024/2/20.
//

#ifndef ANIYA_BOX_V2_MAX31328_H
#define ANIYA_BOX_V2_MAX31328_H

#include <esp_event_base.h>
#include "esp_types.h"
#include "esp_event.h"
#include "time.h"

#include "common_utils.h"

ESP_EVENT_DECLARE_BASE(BIKE_DATE_TIME_SENSOR_EVENT);

// default high level when intr happens cone to low
#define MAX31328_INT_GPIO_NUM 13

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
} max31328_time_t;

typedef struct {
    uint8_t en;
    uint8_t mode; /* 1 day mode 0 week mode */
    uint8_t af;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day_week;
} max31328_alarm_t;

typedef enum {
    MAX31328_SENSOR_INIT_FAILED = 0,
    MAX31328_SENSOR_READ_FAILED,
    MAX31328_ALARM_CONFIG_UPDATE,
    MAX31328_SENSOR_ALARM_INTR1,
    MAX31328_SENSOR_ALARM_INTR2
} max31328_event_id_t;

typedef struct {
    uint8_t data;
} max31328_data_t;

esp_err_t max31328_init();

esp_err_t max31328_deinit();

// year 0-99
// month 1-12
// day 1 - 31
// week 0 Sunday  6 Saturday
// hour 0-23 minute 0-59 second 0-59
esp_err_t
max31328_set_time(uint8_t year, uint8_t month, uint8_t day, uint8_t week, uint8_t hour, uint8_t minute, uint8_t second);

esp_err_t max31328_get_time(uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *week, uint8_t *hour, uint8_t *minute,
                           uint8_t *second);

esp_err_t max31328_get_time_ts(time_t *ts);

// af = alarm status
esp_err_t max31328_load_alarm1(max31328_alarm_t *alarm);

esp_err_t max31328_set_alarm1(max31328_alarm_t *alarm);

esp_err_t max31328_load_alarm2(uint8_t *en, uint8_t *week_mode, uint8_t *af, uint8_t *minute, uint8_t *hour, uint8_t *day_week);

esp_err_t max31328_set_alarm2(uint8_t en, uint8_t week_mode, uint8_t minute, uint8_t hour, uint8_t day_week);

esp_err_t max31328_set_alarm_en(uint8_t alarm1_en, uint8_t alarm2_en);

esp_err_t max31328_read_alarm_status(uint8_t *en1, uint8_t *flag1, uint8_t *en2, uint8_t *flag2);

esp_err_t max31328_clear_alarm_flags(uint8_t alarm1, uint8_t alarm2);

#endif //ANIYA_BOX_V2_MAX31328_H

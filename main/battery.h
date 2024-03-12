#ifndef BATTERY_H
#define BATTERY_H

#define BATTERY_ADC_PWR_GPIO_NUM 18

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(BIKE_BATTERY_EVENT);

typedef enum {
    BATTERY_LEVEL_CHANGE = 0,
} battery_event_id;

void battery_init(void);

// mv
int battery_get_voltage();

// -1 ~ 100  -1 is invalid
int8_t battery_get_level();

bool battery_is_curving();

bool battery_start_curving();

bool battery_is_charge();

uint32_t battery_get_curving_data_count();

void battery_deinit(void);

#endif
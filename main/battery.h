#ifndef BATTERY_H
#define BATTERY_H

#define BATTERY_ADC_PWR_GPIO_NUM 18

void battery_init(void);

// mv
int battery_get_voltage();

// -1 ~ 100  -1 is invalid
int8_t battery_get_level();

bool battery_is_curving();

bool battery_is_charge();

void battery_deinit(void);

#endif
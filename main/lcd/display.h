#ifndef DISPLAY_H
#define DISPLAY_H

#include "key.h"

#define DEEP_SLEEP_TIMEOUT_MS 90000

#define DEFAULT_DISP_ROTATION ROTATE_0

#define DEFAULT_SLEEP_TS 180
#define NIGHT_SLEEP_TS 3600
#define NEVER_SLEEP_TS (-1)
#define NO_SLEEP_TS 1
#define NEVER_WAKE_SLEEP_TS 0

#define DISP_MOSI_GPIO_NUM 4 // sda
#define DISP_MISO_GPIO_NUM -1
#define DISP_CLK_GPIO_NUM 5

ESP_EVENT_DECLARE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

void display_init(uint32_t boot_count);

#endif
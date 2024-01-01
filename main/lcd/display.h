#ifndef DISPLAY_H
#define DISPLAY_H

#include "key.h"

#define DEEP_SLEEP_TIMEOUT_MS 120000

#define DEFAULT_SLEEP_TS 180
#define NEVER_SLEEP_TS (-1)

#define DISP_MOSI_GPIO_NUM 22 // sda
#define DISP_MISO_GPIO_NUM -1
#define DISP_CLK_GPIO_NUM 23
#define DISP_CS_GPIO_NUM -1
#define DISP_DC_GPIO_NUM 20
#define DISP_RST_GPIO_NUM 21
#define DISP_BUSY_GPIO_NUM 19

ESP_EVENT_DECLARE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

void display_init(uint32_t boot_count);

#endif
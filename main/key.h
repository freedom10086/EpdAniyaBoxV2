#ifndef KEY_H
#define KEY_H

#include "esp_types.h"
#include "esp_event.h"
#include "soc/gpio_num.h"

#include "event_common.h"

ESP_EVENT_DECLARE_BASE(BIKE_KEY_EVENT);

#define KEY_COUNT 4

#define KEY_1_NUM 2
#define KEY_2_NUM 0
#define KEY_3_NUM 1
#define KEY_4_NUM 9

static gpio_num_t key_num_list[KEY_COUNT] = {
        KEY_1_NUM,
        KEY_2_NUM,
        KEY_3_NUM,
        KEY_4_NUM
};

/**
 * key click event
 */
typedef enum {
    KEY_1_SHORT_CLICK = 0,
    KEY_1_LONG_CLICK,
    KEY_2_SHORT_CLICK,
    KEY_2_LONG_CLICK,
    KEY_3_SHORT_CLICK,
    KEY_3_LONG_CLICK,
    KEY_4_SHORT_CLICK,
    KEY_4_LONG_CLICK,
} key_event_id_t;

typedef enum {
    KEY_UP_SHORT_CLICK = KEY_1_SHORT_CLICK,
    KEY_UP_LONG_CLICK = KEY_1_LONG_CLICK,
    KEY_DOWN_SHORT_CLICK = KEY_2_SHORT_CLICK,
    KEY_DOWN_LONG_CLICK = KEY_2_LONG_CLICK,
    KEY_CANCEL_SHORT_CLICK = KEY_3_SHORT_CLICK,
    KEY_CANCEL_LONG_CLICK = KEY_3_LONG_CLICK,
    KEY_OK_SHORT_CLICK = KEY_4_SHORT_CLICK,
    KEY_OK_LONG_CLICK = KEY_4_LONG_CLICK,
} human_key_event_id_t;

static key_event_id_t key_event_map[KEY_COUNT][2] = {
        {KEY_1_SHORT_CLICK, KEY_1_LONG_CLICK},
        {KEY_2_SHORT_CLICK, KEY_2_LONG_CLICK},
        {KEY_3_SHORT_CLICK, KEY_3_LONG_CLICK},
        {KEY_4_SHORT_CLICK, KEY_4_LONG_CLICK},
};

void key_init();

#endif
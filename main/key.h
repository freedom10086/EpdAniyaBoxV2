#ifndef KEY_H
#define KEY_H

#include "esp_types.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "soc/gpio_num.h"

ESP_EVENT_DECLARE_BASE(BIKE_KEY_EVENT);

#define KEY_COUNT 2

#define KEY_FN_NUM 2
#define KEY_ENCODER_PUSH_NUM 0
#define KEY_ENCODER_A_NUM 1
#define KEY_ENCODER_B_NUM 9

/**
 * key click event
 */
typedef enum {
    KEY_FN_SHORT_CLICK = 0,
    KEY_FN_LONG_CLICK,
    KEY_FN_DB_CLICK,
    KEY_OK_SHORT_CLICK,
    KEY_OK_LONG_CLICK,
    KEY_OK_DB_CLICK,
    KEY_UP_SHORT_CLICK,
    KEY_DOWN_SHORT_CLICK,
} key_event_id_t;

typedef struct {
    gpio_num_t key_num;
    uint32_t key_down_tick_count;
    uint32_t key_up_tick_count;
    uint8_t state;  // key stage 0 wait key up, 1 wait key down
    esp_timer_handle_t timer[2]; // [0]short timer [1]long timer
    key_event_id_t events[3]; // single long double
    uint8_t key_down_count; // key click down count use to detect double click
} key_state_t;

void key_init();

#endif
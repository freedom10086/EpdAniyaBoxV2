#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <driver/gpio.h>
#include "driver/pulse_cnt.h"
#include <driver/gpio_filter.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "box_common.h"

#include "common_utils.h"
#include "key.h"

#define TAG "keyboard"
#define KEY_CLICK_MIN_GAP 30
#define KEY_SHORT_PRESS_TIME_GAP 100
#define KEY_LONG_PRESS_TIME_GAP 400
#define KEY_ENCODER_TIME_GAP 100

#define PCNT_HIGH_LIMIT 10
#define PCNT_LOW_LIMIT -10

ESP_EVENT_DEFINE_BASE(BIKE_KEY_EVENT);

static key_state_t key_state_list[KEY_COUNT] = {
        {
                KEY_FN_NUM,
                0,
                0,
                1,
                {NULL, NULL},
                {KEY_FN_SHORT_CLICK, KEY_FN_LONG_CLICK, KEY_FN_DB_CLICK},
                0
        },
        {
                KEY_ENCODER_PUSH_NUM,
                0,
                0,
                1,
                {NULL, NULL},
                {KEY_OK_SHORT_CLICK, KEY_OK_LONG_CLICK, KEY_OK_DB_CLICK},
                0
        }
};

static QueueHandle_t event_queue, encoder_event_queue;
static pcnt_unit_handle_t pcnt_unit = NULL;
static esp_timer_handle_t pcnt_timer = NULL;

static uint32_t time_diff_ms, diff_tick, current_tick;
static uint32_t short_click_count;

static void IRAM_ATTR key_gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(event_queue, &gpio_num, NULL);
}

static key_state_t *find_key_state(gpio_num_t gpio_num) {
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        if (key_state_list[i].key_num == gpio_num) {
            return &key_state_list[i];
        }
    }
    ESP_LOGE(TAG, "invalid key gpio num %d", gpio_num);
    return NULL;
}

static void short_timer_callback(void *arg) {
    gpio_num_t gpio_num = (gpio_num_t) arg;
    key_state_t *state = find_key_state(gpio_num);
    if (state == NULL) {
        return;
    }

    ESP_LOGI(TAG, "short timer call back timer for gpio: %d", state->key_num);
    // still not down
    if (gpio_get_level(gpio_num) == 1) {
        time_diff_ms = pdTICKS_TO_MS(xTaskGetTickCount() - state->key_down_tick_count);
        ESP_LOGI(TAG, "short timer callback time_diff_ms %ld", time_diff_ms);
        if (time_diff_ms >= KEY_SHORT_PRESS_TIME_GAP - 10) {
            if (state->key_down_count == 1) {
                // short pressed
                ESP_LOGI(TAG, "key %d short press detect by timer", gpio_num);
                common_post_event_data(BIKE_KEY_EVENT,
                                       state->events[0],
                                       &time_diff_ms,
                                       sizeof(time_diff_ms));
            } else {
                // db pressed
                ESP_LOGI(TAG, "key %d double press detect by timer", gpio_num);
                common_post_event_data(BIKE_KEY_EVENT,
                                       state->events[2],
                                       &time_diff_ms,
                                       sizeof(time_diff_ms));
            }
            state->key_down_count = 0;
        }
        state->state = 1;
    }
}

static void long_timer_callback(void *arg) {
    gpio_num_t gpio_num = (gpio_num_t) arg;
    key_state_t *state = find_key_state(gpio_num);
    if (state == NULL) {
        return;
    }

    ESP_LOGI(TAG, "long timer call back timer for gpio: %d", state->key_num);
    // still down
    if (gpio_get_level(gpio_num) == 0) {
        time_diff_ms = pdTICKS_TO_MS(xTaskGetTickCount() - state->key_down_tick_count);
        ESP_LOGI(TAG, "timer callback time_diff_ms %ld", time_diff_ms);
        if (time_diff_ms >= KEY_LONG_PRESS_TIME_GAP - 10) {
            // long pressed
            ESP_LOGI(TAG, "key %d long press detect by timer", gpio_num);
            common_post_event_data(BIKE_KEY_EVENT,
                                   state->events[1],
                                   &time_diff_ms,
                                   sizeof(time_diff_ms));
        }
        state->state = 1;
    }
}

static void key_task_entry(void *arg) {
    event_queue = xQueueCreate(10, sizeof(gpio_num_t));
    for (int i = 0; i < KEY_COUNT; ++i) {
        char buf[6];
        sprintf(buf, "key_%d", i);

        esp_timer_create_args_t timer_args = {
                .arg = (void *) key_state_list[i].key_num,
                .callback = &short_timer_callback,
                .name = buf
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &key_state_list[i].timer[0]));

        timer_args.callback = &long_timer_callback;
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &key_state_list[i].timer[1]));
    }

    gpio_num_t num = box_get_wakeup_ionum();

    key_state_t *wakeup_state = find_key_state(num);
    if (wakeup_state != NULL) {
        xQueueSend(event_queue, &num, 0);
    }

    gpio_num_t clicked_gpio;
    while (1) {
        if (xQueueReceive(event_queue, &clicked_gpio, portMAX_DELAY)) {
            current_tick = xTaskGetTickCount();
            key_state_t *key_state = find_key_state(clicked_gpio);
            gpio_intr_disable(clicked_gpio);
            bool key_down = gpio_get_level(clicked_gpio) == 0;

            if (key_down && key_state->state == 1) { // key down
                //ESP_LOGI(TAG, "key %d click down detect...", clicked_gpio);
                if (esp_timer_is_active(key_state->timer[1])) {
                    esp_timer_stop(key_state->timer[1]);
                }

                key_state->state = 0;
                key_state->key_down_tick_count = current_tick;
                key_state->key_down_count ++;

                // start timer for check long click
                esp_timer_start_once(key_state->timer[1], KEY_LONG_PRESS_TIME_GAP * 1000);
            } else if (!key_down && key_state->state == 0) { // key up
                // cancel long click detect timer
                if (esp_timer_is_active(key_state->timer[1])) {
                    esp_timer_stop(key_state->timer[1]);
                }

                // if no dbclick useing fast mode
                if (key_state->events[2] == 0) {
                    //ESP_LOGI(TAG, "key %d click up detect...", clicked_gpio);
                    diff_tick = current_tick - key_state->key_down_tick_count;
                    if (pdTICKS_TO_MS(diff_tick) >= KEY_LONG_PRESS_TIME_GAP) {

                        ESP_LOGI(TAG, "key %d long press", clicked_gpio);
                        common_post_event_data(BIKE_KEY_EVENT,
                                               key_state->events[1],
                                               &time_diff_ms,
                                               sizeof(time_diff_ms));
                    } else if (pdTICKS_TO_MS(diff_tick) >= KEY_CLICK_MIN_GAP) {
                        ESP_LOGI(TAG, "key %d short press cnt:%ld", clicked_gpio, short_click_count++);
                        common_post_event_data(BIKE_KEY_EVENT,
                                               key_state->events[0],
                                               &time_diff_ms,
                                               sizeof(time_diff_ms));
                    } else {
                        ESP_LOGW(TAG, "key up too quickly gpio:%d, time diff:%ldms", clicked_gpio,
                                 pdTICKS_TO_MS(diff_tick));
                    }
                } else {
                    // start db click detect or single click timer
                    // cancel long click detect timer
                    if (esp_timer_is_active(key_state->timer[0])) {
                        esp_timer_stop(key_state->timer[0]);
                    }

                    // start timer for check single/double click
                    esp_timer_start_once(key_state->timer[0], KEY_SHORT_PRESS_TIME_GAP * 1000);
                }

                key_state->state = 1;
                key_state->key_up_tick_count = current_tick;
            } else {
                ESP_LOGW(TAG, "key %d invalid state, level:%d state:%d", clicked_gpio, !key_down, key_state->state);
            }

            gpio_intr_enable(clicked_gpio);
        }
    }

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        esp_timer_stop(key_state_list[i].timer);
        esp_timer_delete(key_state_list[i].timer);
    }

    vTaskDelete(NULL);
}

static void encoder_timer_callback(void *arg) {
    // do nothing
}

static void encoder_task_entry(void *arg) {
    esp_timer_create_args_t timer_args = {
            .arg = (void *) NULL,
            .callback = &encoder_timer_callback,
            .name = "encoder_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &pcnt_timer));

    // Report counter value
    int last_pulse_count = 0;
    int curr_pulse_count = 0;
    while (1) {
        if (xQueueReceive(encoder_event_queue, &curr_pulse_count, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "Watch point event, Pulse count: %d", curr_pulse_count);
            if (esp_timer_is_active(pcnt_timer)) {
                // time not ok
                // esp_timer_stop(pcnt_timer);
            } else {
                bool direction = curr_pulse_count - last_pulse_count;
                key_event_id_t evt = direction > 0 ? KEY_UP_SHORT_CLICK : KEY_DOWN_SHORT_CLICK;
                common_post_event_data(BIKE_KEY_EVENT,
                                       evt,
                                       (void *) (curr_pulse_count - last_pulse_count),
                                       sizeof(int));
                esp_timer_start_once(pcnt_timer, KEY_ENCODER_TIME_GAP * 1000);
            }

            last_pulse_count = curr_pulse_count;
        } else {
            //ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &curr_pulse_count));
            //ESP_LOGI(TAG, "Pulse count: %d", curr_pulse_count);

        }
    }
}

static bool pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup;
    QueueHandle_t queue = (QueueHandle_t) user_ctx;
    // send event data to queue, from this interrupt callback
    xQueueSendFromISR(queue, &(edata->watch_point_value), &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}


void rotary_encoder_init() {
    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
            .high_limit = PCNT_HIGH_LIMIT,
            .low_limit = PCNT_LOW_LIMIT,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    ESP_LOGI(TAG, "install pcnt channels");
    pcnt_chan_config_t chan_a_config = {
            .edge_gpio_num = KEY_ENCODER_A_NUM,
            .level_gpio_num = KEY_ENCODER_B_NUM,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    pcnt_chan_config_t chan_b_config = {
            .edge_gpio_num = KEY_ENCODER_B_NUM,
            .level_gpio_num = KEY_ENCODER_A_NUM,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_LOGI(TAG, "add watch points and register callbacks");
    int watch_points[] = {PCNT_LOW_LIMIT, -8, -6, -4, -2, 0, 2, 4, 6, 8, PCNT_HIGH_LIMIT};
    for (size_t i = 0; i < sizeof(watch_points) / sizeof(watch_points[0]); i++) {
        ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, watch_points[i]));
    }
    pcnt_event_callbacks_t cbs = {
            .on_reach = pcnt_on_reach,
    };
    encoder_event_queue = xQueueCreate(10, sizeof(int));
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, encoder_event_queue));

    ESP_LOGI(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_LOGI(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_LOGI(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    TaskHandle_t tsk_hdl;
    /* Create key click detect task */
    BaseType_t err = xTaskCreate(
            encoder_task_entry,
            "encoder_task",
            3072,
            NULL,
            uxTaskPriorityGet(NULL),
            &tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create encoder detect task failed");
    }

    ESP_LOGI(TAG, "encoder task init OK");
}

void key_init() {
    TaskHandle_t tsk_hdl;
    /* Create key click detect task */
    BaseType_t err = xTaskCreate(
            key_task_entry,
            "key_task",
            3072,
            NULL,
            uxTaskPriorityGet(NULL),
            &tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create key detect task failed");
    }

    ESP_LOGI(TAG, "keyboard task init OK");

    // bit_mask = (1ull << KEY_1_NUM) | (1ull << KEY_2_NUM) | (1ull << KEY_3_NUM) | (1ull << KEY_4_NUM),
    uint64_t bit_mask = 0;
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        bit_mask |= (1ull << key_state_list[i].key_num);
    }

    gpio_config_t io_config = {
            .pin_bit_mask = bit_mask,
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_ANYEDGE,
            .pull_up_en = 1,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    //ESP_LOGI(TAG, "io %d, level %d", KEY_1_NUM, gpio_get_level(KEY_1_NUM));
    //ESP_LOGI(TAG, "io %d, level %d", KEY_2_NUM, gpio_get_level(KEY_2_NUM));

    gpio_pin_glitch_filter_config_t gpio_filter_config;
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        gpio_filter_config.clk_src = SOC_MOD_CLK_PLL_F96M;
        gpio_filter_config.gpio_num = key_state_list[i].key_num;
        gpio_glitch_filter_handle_t gpio_filter_handle;
        gpio_new_pin_glitch_filter(&gpio_filter_config, &gpio_filter_handle);
        gpio_glitch_filter_enable(gpio_filter_handle);
    }

    //install gpio isr service
    //gpio_install_isr_service(0);

    //hook isr handler for specific gpio pin
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        gpio_isr_handler_add(key_state_list[i].key_num, key_gpio_isr_handler, (void *) key_state_list[i].key_num);
    }

    ESP_LOGI(TAG, "inited");
}
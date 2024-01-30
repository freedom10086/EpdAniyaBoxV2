#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <driver/gpio.h>
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
#define KEY_LONG_PRESS_TIME_GAP 400

ESP_EVENT_DEFINE_BASE(BIKE_KEY_EVENT);

static key_state_t key_state_list[KEY_COUNT] = {
        {
                KEY_1_NUM,
                0,
                0,
                1,
                NULL,
                {KEY_1_SHORT_CLICK, KEY_1_LONG_CLICK},
        },
        {
                KEY_2_NUM,
                0,
                0,
                1,
                NULL,
                {KEY_2_SHORT_CLICK, KEY_2_LONG_CLICK},
        },
        {
                KEY_3_NUM,
                0,
                0,
                1,
                NULL,
                {KEY_3_SHORT_CLICK, KEY_3_LONG_CLICK},
        },
        {
                KEY_4_NUM,
                0,
                0,
                1,
                NULL,
                {KEY_4_SHORT_CLICK, KEY_4_LONG_CLICK},
        }
};

static QueueHandle_t event_queue;

static uint32_t time_diff_ms, diff_tick, current_tick;
static uint32_t short_click_count;

static void IRAM_ATTR key_gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(event_queue, &gpio_num, NULL);
}

static key_state_t* find_key_state(gpio_num_t gpio_num) {
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        if (key_state_list[i].key_num == gpio_num) {
            return &key_state_list[i];
        }
    }
    ESP_LOGE(TAG, "invalid key gpio num %d", gpio_num);
    return NULL;
}

static void timer_callback(void *arg) {
    gpio_num_t gpio_num = (gpio_num_t) arg;
    key_state_t* state = find_key_state(gpio_num);
    if (state == NULL) {
        return;
    }

    ESP_LOGI(TAG, "timer call back timer for gpio: %d", state->key_num);
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
        state ->state = 1;
    }
}

static void key_task_entry(void *arg) {
    event_queue = xQueueCreate(10, sizeof(gpio_num_t));
    for (int i = 0; i < KEY_COUNT; ++i) {
        char buf[6];
        sprintf(buf, "key_%d", i);

        esp_timer_create_args_t timer_args = {
                .arg = (void *) key_state_list[i].key_num,
                .callback = &timer_callback,
                .name = buf
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &key_state_list[i].timer));
    }

    gpio_num_t num = box_get_wakeup_ionum();

    key_state_t* wakeup_state = find_key_state(num);
    if (wakeup_state != NULL) {
        xQueueSend(event_queue, &num, 0);
    }

    gpio_num_t clicked_gpio;
    while (1) {
        if (xQueueReceive(event_queue, &clicked_gpio, portMAX_DELAY)) {
            current_tick = xTaskGetTickCount();
            key_state_t* key_state = find_key_state(clicked_gpio);
            gpio_intr_disable(clicked_gpio);
            bool key_down = gpio_get_level(clicked_gpio) == 0;

            if (key_down && key_state->state == 1) { // key down
                //ESP_LOGI(TAG, "key %d click down detect...", clicked_gpio);
                if (esp_timer_is_active(key_state->timer)) {
                    esp_timer_stop(key_state->timer);
                }

                key_state->state = 0;
                key_state->key_down_tick_count = current_tick;

                // start timer for check long click
                esp_timer_start_once(key_state->timer, KEY_LONG_PRESS_TIME_GAP * 1000);
            } else if (!key_down && key_state->state == 0) { // key up
                if (esp_timer_is_active(key_state->timer)) {
                    esp_timer_stop(key_state->timer);
                }

                //ESP_LOGI(TAG, "key %d click up detect...", clicked_gpio);
                diff_tick = current_tick - key_state->key_down_tick_count;
                if (time_diff_ms > KEY_LONG_PRESS_TIME_GAP) {

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
                    ESP_LOGW(TAG, "key up too quickly gpio:%d, time diff:%ldms", clicked_gpio, pdTICKS_TO_MS(diff_tick));
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
        gpio_filter_config.clk_src = SOC_MOD_CLK_PLL_F80M;
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
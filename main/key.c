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

#include "common_utils.h"
#include "key.h"


#define TAG "keyboard"
#define KEY_CLICK_MIN_GAP 30
#define KEY_LONG_PRESS_TIME_GAP 400

ESP_EVENT_DEFINE_BASE(BIKE_KEY_EVENT);

static QueueHandle_t event_queue;
static esp_timer_handle_t timers[KEY_COUNT];
static uint32_t tick_count[KEY_COUNT];
static uint32_t key_up_tick_count[KEY_COUNT];
static bool ignore_long_pressed[KEY_COUNT];

static uint32_t time_diff_ms, tick_diff, key_up_tick_diff;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(event_queue, &gpio_num, NULL);
}

static uint8_t key_gpio_num_to_index(gpio_num_t gpio_num) {
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        if (key_num_list[i] == gpio_num) {
            return i;
        }
    }
    ESP_LOGE(TAG, "invalid key gpio num %d", gpio_num);
    return -1;
}

static void timer_callback(void *arg) {
    gpio_num_t gpio_num = (gpio_num_t) arg;
    uint8_t index = key_gpio_num_to_index(gpio_num);

    ESP_LOGI(TAG, "timer call back timer for %d", index);
    // still down
    if (gpio_get_level(gpio_num) == 0) {
        time_diff_ms = pdTICKS_TO_MS(xTaskGetTickCount() - tick_count[index]);
        ESP_LOGI(TAG, "timer call back time_diff_ms %ld", time_diff_ms);
        if (time_diff_ms >= KEY_LONG_PRESS_TIME_GAP - 10) {
            // long pressed
            ignore_long_pressed[index] = true;
            ESP_LOGI(TAG, "key %d(io: %d) long press detect by timer", index, gpio_num);
            common_post_event_data(BIKE_KEY_EVENT,
                                   key_event_map[index][1],
                                   &time_diff_ms,
                                   sizeof(time_diff_ms));
        }
    }
}

static void key_task_entry(void *arg) {
    event_queue = xQueueCreate(10, sizeof(gpio_num_t));
    for (int i = 0; i < KEY_COUNT; ++i) {
        char buf[6];
        sprintf(buf, "key_%d", i);

        esp_timer_create_args_t timer_args = {
                .arg = (void *) key_num_list[i],
                .callback = &timer_callback,
                .name = buf
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timers[i]));
    }

    gpio_num_t clicked_gpio;
    while (1) {
        if (xQueueReceive(event_queue, &clicked_gpio, portMAX_DELAY)) {
            // vTaskDelay(pdMS_TO_TICKS(2));
            uint8_t index = key_gpio_num_to_index(clicked_gpio);
            if (gpio_get_level(clicked_gpio) == 0) {
                // key down
                //ESP_LOGI(TAG, "key %d click down detect...", clicked_gpio);
                if (!esp_timer_is_active(timers[index])) {
                    ignore_long_pressed[index] = false;
                    ESP_LOGI(TAG, "start timer for %d", index);
                    esp_timer_start_once(timers[index], KEY_LONG_PRESS_TIME_GAP * 1000);
                }
            } else if (gpio_get_level(clicked_gpio) == 1) {
                // key up
                //ESP_LOGI(TAG, "key %d click up detect...", clicked_gpio);
                tick_diff = xTaskGetTickCount() - tick_count[index];
                time_diff_ms = pdTICKS_TO_MS(tick_diff);

                key_up_tick_diff = xTaskGetTickCount() - key_up_tick_count[index];
                if (time_diff_ms > KEY_LONG_PRESS_TIME_GAP) {
                    if (ignore_long_pressed[index]) {
                        ESP_LOGI(TAG, "key %d long press, ignore", clicked_gpio);
                    } else {
                        ESP_LOGI(TAG, "key %d(io: %d) long press", index, clicked_gpio);
                        common_post_event_data(BIKE_KEY_EVENT,
                                               key_event_map[index][1],
                                               &time_diff_ms,
                                               sizeof(time_diff_ms));
                    }
                } else if (pdTICKS_TO_MS(key_up_tick_diff) > KEY_CLICK_MIN_GAP) {
                    if (esp_timer_is_active(timers[index])) {
                        esp_timer_stop(timers[index]);
                    }

                    ESP_LOGI(TAG, "key %d(io: %d) short press", index, clicked_gpio);
                    common_post_event_data(BIKE_KEY_EVENT,
                                           key_event_map[index][0],
                                           &time_diff_ms,
                                           sizeof(time_diff_ms));
                } else {
                    ESP_LOGW(TAG, "key up to quickly gpio:%d, time diff:%ldms", clicked_gpio,
                             pdTICKS_TO_MS(key_up_tick_diff));
                }

                key_up_tick_count[index] = xTaskGetTickCount();
            }

            tick_count[index] = xTaskGetTickCount();
        }
    }

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        esp_timer_stop(timers[i]);
        esp_timer_delete(timers[i]);
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
        bit_mask |= (1ull << key_num_list[i]);
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
        gpio_filter_config.gpio_num = key_num_list[i];
        gpio_glitch_filter_handle_t gpio_filter_handle;
        gpio_new_pin_glitch_filter(&gpio_filter_config, &gpio_filter_handle);
        gpio_glitch_filter_enable(gpio_filter_handle);
    }

    //install gpio isr service
    gpio_install_isr_service(0);

    //hook isr handler for specific gpio pin
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        gpio_isr_handler_add(key_num_list[i], gpio_isr_handler, (void *) key_num_list[i]);
    }

    ESP_LOGI(TAG, "keyboard isr detect add OK");
}
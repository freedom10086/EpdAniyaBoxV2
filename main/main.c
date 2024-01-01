#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver/rtc_io.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "box_common.h"
#include "lcd/display.h"
#include "sht31.h"
#include "key.h"
#include "battery.h"
#include "common_utils.h"
#include "driver/i2c_master.h"
#include "LIS3DH.h"
#include "rx8025t.h"
#include "sht31.h"
#include "beep/beep.h"
#include "ws2812.h"

static const char *TAG = "BIKE_MAIN";
#define I2C_MASTER_NUM              0
#define I2C_SCL_IO 11
#define I2C_SDA_IO 8

esp_event_loop_handle_t event_loop_handle;
i2c_master_bus_handle_t i2c_bus_handle;

RTC_DATA_ATTR static uint32_t boot_count = 0;

static esp_err_t i2c_master_init(void);

static void application_task(void *args) {
    while (1) {
        esp_err_t err = esp_event_loop_run(event_loop_handle, portMAX_DELAY);
        if (err != ESP_OK) {
            break;
        }
    }

    ESP_LOGE(TAG, "suspended task for loop %p", event_loop_handle);
    vTaskSuspend(NULL);
}

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    //esp_log_level_set("*", ESP_LOG_WARN);
    //esp_log_level_set("battery", ESP_LOG_WARN);
    //esp_log_level_set("lcd_panel.ssd1680", ESP_LOG_WARN);
    //esp_log_level_set("keyboard", ESP_LOG_WARN);
    //esp_log_level_set("display", ESP_LOG_WARN);

    boot_count++;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    printf("Hello world!, boot count %ld wake up cause:%d\n", boot_count, cause);

    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("Wake up from timer.");
            break;
        }
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("Wake up from GPIO %d\n", pin);
            } else {
                printf("Wake up from GPIO\n");
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_GPIO: {
            uint64_t wakeup_pin_mask = esp_sleep_get_gpio_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("Wake up from GPIO %d\n", pin);
            } else {
                printf("Wake up from GPIO\n");
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");
    }

    // create event loop
    esp_event_loop_args_t loop_args = {
            .queue_size = 16,
            .task_name = NULL // no task will be created
    };

    // Create the event loops
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &event_loop_handle));

    //esp_event_loop_delete(esp_gps->event_loop_hdl);

    ESP_LOGI(TAG, "starting application task");
    // Create the application task with the same priority as the current task
    xTaskCreate(application_task, "application_task", 3072, NULL, uxTaskPriorityGet(NULL), NULL);

    /**
     * init iic
     */
    i2c_master_init();

    esp_err_t lis3dh_error = lis3dh_init(LIS3DH_LOW_POWER_MODE, LIS3DH_ACC_RANGE_2,
                                         LIS3DH_ACC_SAMPLE_RATE_25);
    if (lis3dh_error == ESP_OK) {
        ESP_LOGI(TAG, "lis3dh init success");
    } else {
        ESP_LOGW(TAG, "lis3dh init failed");
    }

    lis3dh_config_motion_detect();

    float accx, accy, accz;
    for (int i = 0; i < 5; ++i) {
        lis3dh_error = lis3dh_read_acc(&accx, &accy, &accz);
        if (lis3dh_error == ESP_OK) {
            ESP_LOGI(TAG, "lis3dh read acc x:%.2f y:%.2f z:%.2f", accx, accy, accz);
        } else {
            ESP_LOGW(TAG, "lis3dh read acc failed");
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    lis3dh_mode_t mode;
    lis3dh_acc_range_t acc_range;
    lis3dh_acc_sample_rage_t acc_sample_rate;
    lis3dh_current_mode(&mode, &acc_range, &acc_sample_rate);
    ESP_LOGI(TAG, "lis3dh mode:%d, range:%d, rate:%d", mode, acc_range, acc_sample_rate);

    esp_err_t err = rx8025t_init();
//    err = rx8025t_set_time(23, 12, 18, 1, 18, 51, 00);
//    if (err == ESP_OK) {
//        ESP_LOGI(TAG, "set time ok");
//    } else {
//        ESP_LOGW(TAG, "set time failed");
//    }

    uint8_t year, month, day, week, hour, minute, second;
    err = rx8025t_get_time(&year, &month, &day, &week, &hour, &minute, &second);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "read time: %d-%d-%d %d:%d:%d week:%d", year, month, day, hour, minute, second, week);
    } else {
        ESP_LOGW(TAG, "read time failed");
    }

    /**
     * key
     */
    key_init();

    /**
     * battery detect
     */
    battery_init();

    /**
     * ws2812
     */
    //ws2812_init();
    //ws2812_show_color();

    /**
     * lcd
     */
    display_init(boot_count);

    // beep
    //beep_init();
    //start_beep(1000);
    //vTaskDelay(pdMS_TO_TICKS(1000));
    //stop_beep();
}

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_master_bus_config_t i2c_mst_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = i2c_master_port,
            .scl_io_num = I2C_SCL_IO,
            .sda_io_num = I2C_SDA_IO,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
    };

    esp_err_t iic_err = i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle);
    if (iic_err != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialized failed %d %s", iic_err, esp_err_to_name(iic_err));
    } else {
        ESP_LOGI(TAG, "I2C initialized successfully");
    }
    return iic_err;
}

void box_enter_deep_sleep(int sleep_ts) {
    if (sleep_ts > 0) {
        esp_sleep_enable_timer_wakeup(sleep_ts * 1000000);
    } else if (sleep_ts == 0) {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }

#if SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN
    // EXT1_WAKEUP
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << KEY_1_NUM, ESP_GPIO_WAKEUP_GPIO_LOW));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << KEY_2_NUM, ESP_GPIO_WAKEUP_GPIO_LOW));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << KEY_3_NUM, ESP_GPIO_WAKEUP_GPIO_LOW));

    // if no external pull-up/downs
    ESP_ERROR_CHECK(rtc_gpio_pullup_en(KEY_1_NUM));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(KEY_1_NUM));

    ESP_ERROR_CHECK(rtc_gpio_pullup_en(KEY_2_NUM));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(KEY_2_NUM));

    ESP_ERROR_CHECK(rtc_gpio_pullup_en(KEY_3_NUM));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(KEY_3_NUM));

    // motion wake up
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << IMU_INT_1_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << IMU_INT_2_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH));
#else
    // gpio wake up
    const gpio_config_t config = {
            .pin_bit_mask = 1 << KEY_1_NUM | 1 << KEY_2_NUM | 1 << KEY_3_NUM,
            .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(config.pin_bit_mask, ESP_GPIO_WAKEUP_GPIO_LOW));
#endif
    ESP_LOGI(TAG, "enter deep sleep mode, sleep %ds", sleep_ts);
    esp_deep_sleep_start();
}
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
#include "sht40.h"
#include "key.h"
#include "battery.h"
#include "common_utils.h"
#include "driver/i2c_master.h"
#include "LIS3DH.h"
#include "max31328.h"
#include "sht40.h"
#include "beep/beep.h"

static const char *TAG = "BIKE_MAIN";
#define I2C_MASTER_NUM              0
#define I2C_SCL_IO 11
#define I2C_SDA_IO 8

i2c_master_bus_handle_t i2c_bus_handle;

RTC_DATA_ATTR uint32_t boot_count = 0;

static esp_err_t i2c_master_init(void);


/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    //esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("battery", ESP_LOG_WARN);
    // esp_log_level_set("keyboard", ESP_LOG_WARN);
    esp_log_level_set("LIS3DH", ESP_LOG_WARN);
    esp_log_level_set("page-manager", ESP_LOG_WARN);

    boot_count++;
    box_get_wakeup_ionum();

    // use system event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //install gpio isr service
    gpio_install_isr_service(0);

    /**
     * init iic
     */
    i2c_master_init();

    /**
     * key
     */
    key_init();

    /**
     * battery detect
     */
    battery_init();

    /**
     * lcd
     */
    display_init(boot_count);

    // max31328
    max31328_init();

    vTaskDelay(pdMS_TO_TICKS(1000));
    lis3dh_init(LIS3DH_LOW_POWER_MODE, LIS3DH_ACC_RANGE_2,LIS3DH_ACC_SAMPLE_RATE_25);
    lis3dh_config_motion_detect();
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
        esp_sleep_enable_timer_wakeup((uint64_t)sleep_ts * 1000000);
    } else if (sleep_ts == 0) {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }

#if SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN
    // EXT1_WAKEUP
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << KEY_FN_NUM, ESP_EXT1_WAKEUP_ANY_LOW));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << KEY_ENCODER_PUSH_NUM, ESP_EXT1_WAKEUP_ANY_LOW));

    // motion wake up
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << IMU_INT_1_GPIO, ESP_EXT1_WAKEUP_ANY_HIGH));

    // alarm wake up
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << MAX31328_INT_GPIO_NUM, ESP_EXT1_WAKEUP_ANY_LOW));
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

gpio_num_t box_get_wakeup_ionum() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    printf("Hello world!, boot count %ld wake up cause:%d\n", boot_count, cause);
    uint64_t wakeup_pin_mask = 0;
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("Wake up from timer.");
            break;
        }
        case ESP_SLEEP_WAKEUP_EXT1: {
            wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            break;
        }
#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
        case ESP_SLEEP_WAKEUP_GPIO: {
            wakeup_pin_mask = esp_sleep_get_gpio_wakeup_status();
            break;
        }
#endif
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");
    }

    if (wakeup_pin_mask > 0) {
        int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
        printf("Wake up from GPIO %d\n", pin);
        return pin;
    }
    return GPIO_NUM_NC;
}
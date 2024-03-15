//
// Created by yang on 2024/2/20.
//

#include "max31328.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "beep/beep.h"
#include "key.h"
#include "LIS3DH.h"
#include "esp_types.h"
#include "esp_event.h"

#define I2C_MASTER_NUM              0
#define I2C_MASTER_TIMEOUT_MS       200

#define MAX31328_ADDR                 0b1101000

#define ADDR_SEC    0x00
#define ADDR_MIN    0x01
#define ADDR_HOUR   0x02
#define ADDR_WEEK   0x03
#define ADDR_DAY    0x04
#define ADDR_MONTH    0x05
#define ADDR_YEAR    0x06

#define ADDR_SEC_ALARM      0x07
#define ADDR_MIN_ALARM      0x08
#define ADDR_HOUR_ALARM     0x09
#define ADDR_WEEK_ALARM     0x0A
#define ADDR_DAY_ALARM      0x0A

#define ADDR_MIN_ALARM2      0x0B
#define ADDR_HOUR_ALARM2     0x0C
#define ADDR_WEEK_ALARM2     0x0D
#define ADDR_DAY_ALARM2      0x0D

#define ADDR_CONTROL        0x0E
#define ADDR_STATUS         0x0F

#define ADDR_AGING_OFFSET   0x10
#define ADDR_TEMP_MSB       0x11
#define ADDR_TEMP_LSB       0x12

ESP_EVENT_DEFINE_BASE(BIKE_DATE_TIME_SENSOR_EVENT);

#define TAG "max31328"

extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;
static bool max31328_inited = false;

static TaskHandle_t tsk_hdl;

static uint8_t hex2bcd(uint8_t x) {
    uint8_t y;
    y = (x / 10) << 4;
    y = y | (x % 10);
    return (y);
}

static uint8_t bcd2hex(uint8_t bcd) {
    uint8_t dec = 0;
    uint8_t mult;
    for (mult = 1; bcd; bcd = bcd >> 4, mult *= 10) {
        dec += (bcd & 0x0f) * mult;
    }
    return dec;
}

static esp_err_t i2c_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev_handle, &reg_addr,
                                       1, data, len,
                                       I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t i2c_write_byte(uint8_t reg_addr, uint8_t data) {
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
    return ret;
}

static esp_err_t i2c_write(const uint8_t *write_buffer, size_t write_size) {
    int ret;
    ret = i2c_master_transmit(dev_handle, write_buffer, write_size, I2C_MASTER_TIMEOUT_MS);

    return ret;
}

static void IRAM_ATTR max31328_gpio_isr_handler(void *arg) {
    vTaskGenericNotifyGiveFromISR(tsk_hdl, 0, NULL);
}

static void config_intr_gpio() {
    gpio_config_t io_config = {
            .pin_bit_mask = (1ull << MAX31328_INT_GPIO_NUM),
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_NEGEDGE,
            .pull_up_en = 0,
            .pull_down_en = 0,
    };

    ESP_ERROR_CHECK(gpio_config(&io_config));

    //install gpio isr service
    //gpio_install_isr_service(0);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(MAX31328_ADDR, max31328_gpio_isr_handler, (void *) MAX31328_INT_GPIO_NUM);
    ESP_LOGI(TAG, "rx8025t gpio isr add OK");
}

static void stop_alarm_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    stop_beep();
}

static void max31328_task_entry(void *arg) {
    // wait display ok
    // vTaskDelay(pdMS_TO_TICKS(300));
    config_intr_gpio();

    while (true) {
        // read af
        uint8_t af1 = 0, af2 = 0, en1 = 0, en2 = 0;
        max31328_read_alarm_status(&en1, &af1, &en2, &af2);

        ESP_LOGI(TAG, "flags en1:%d af1:%d en2:%d, af2:%d", en1, af1, en2, af2);

        // int_gpio_level == 0
        // isr happens
        gpio_intr_disable(MAX31328_INT_GPIO_NUM);

        if (gpio_get_level(MAX31328_INT_GPIO_NUM) == 0) {
            ESP_LOGI(TAG, "isr happens gpio %d goes low", MAX31328_INT_GPIO_NUM);
        }

        // clear af
        if (af1 || af2) {
            max31328_clear_alarm_flags(af1, af2);
        }

        if ((af1 && en1) || (af2 && en2)) {
            ESP_LOGI(TAG, "== af isr happens ==");
            // read time check alarm is valid
            esp_event_handler_register(BIKE_KEY_EVENT, ESP_EVENT_ANY_ID, stop_alarm_handler,
                                            NULL);
            esp_event_handler_register(BIKE_MOTION_EVENT, ESP_EVENT_ANY_ID,
                                            stop_alarm_handler, NULL);

            if (af1 && en1) {
                common_post_event(BIKE_DATE_TIME_SENSOR_EVENT, MAX31328_SENSOR_ALARM_INTR1);
            }

            if (af2 && en2) {
                common_post_event(BIKE_DATE_TIME_SENSOR_EVENT, MAX31328_SENSOR_ALARM_INTR2);
            }

            ESP_LOGI(TAG, "start play alarm music...");
            beep_init(BEEP_MODE_RMT);
            beep_start_play(music_score_hszy, sizeof(music_score_hszy) / sizeof(music_score_hszy[0]));
            beep_deinit();
            ESP_LOGI(TAG, "play alarm music done!");

            esp_event_handler_unregister(BIKE_KEY_EVENT, ESP_EVENT_ANY_ID,
                                              stop_alarm_handler);
            esp_event_handler_unregister(BIKE_MOTION_EVENT, ESP_EVENT_ANY_ID,
                                              stop_alarm_handler);
        }

        // clear all holding notification
        ulTaskGenericNotifyTake(0, pdTRUE, 0);
        gpio_intr_enable(MAX31328_INT_GPIO_NUM);

        uint32_t notify_value = ulTaskGenericNotifyTake(0, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "get notify value %ld", notify_value);
    }

    vTaskDelete(NULL);
}

esp_err_t max31328_init() {
    if (max31328_inited) {
        return ESP_OK;
    }

    setenv("TZ", "CST-8", 1);
    tzset();

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = MAX31328_ADDR,
            .scl_speed_hz = 200000,
    };

    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t create_task_err = xTaskCreate(
            max31328_task_entry,
            "max31328_task",
            3072,
            NULL,
            0,
            &tsk_hdl);
    if (create_task_err != pdTRUE) {
        ESP_LOGE(TAG, "create rx8025 alarm detect task failed");
    }

    max31328_inited = true;
    ESP_LOGI(TAG, "init success!");
    return ESP_OK;
}

esp_err_t max31328_deinit() {
    if (!max31328_inited) {
        return ESP_OK;
    }

    vTaskDelete(tsk_hdl);

    gpio_isr_handler_remove(MAX31328_INT_GPIO_NUM);

    max31328_inited = false;
    ESP_LOGI(TAG, "deinit success!");
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t
max31328_set_time(uint8_t year, uint8_t month, uint8_t day, uint8_t week, uint8_t hour, uint8_t minute, uint8_t second) {
    max31328_init();

    year = year % 2000;
    // year 0-99
    // month 1-12
    // day 1 - 31
    // week 0 Sunday  6 Saturday
    // hour 0-23 24hours format
    // minute 0-59 second 0-59

    uint8_t write_buf[] = {ADDR_SEC, hex2bcd(second), hex2bcd(minute), hex2bcd(hour),
                           week,
                           hex2bcd(day), hex2bcd(month), hex2bcd(year)};

    return i2c_write(write_buf, sizeof(write_buf));
}

esp_err_t max31328_get_time(uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *week, uint8_t *hour, uint8_t *minute,
                           uint8_t *second) {
    max31328_init();

    uint8_t read_buf[7];
    esp_err_t err = i2c_read_reg(ADDR_SEC, read_buf, sizeof(read_buf));
    if (err == ESP_OK) {
        *year = bcd2hex(read_buf[6]);
        *month = bcd2hex(read_buf[5]);
        *day = bcd2hex(read_buf[4]);
        *week = read_buf[3];
        *hour = bcd2hex(read_buf[2]);
        *minute = bcd2hex(read_buf[1]);
        *second = bcd2hex(read_buf[0]);
    }

    return err;
}

esp_err_t max31328_get_time_ts(time_t *ts) {
    max31328_init();
    max31328_time_t t;
    esp_err_t err = max31328_get_time(&t.year, &t.month, &t.day, &t.week, &t.hour, &t.minute, &t.second);
    if (err != ESP_OK) {
        return err;
    }

    struct tm time = {0};
    time.tm_year = t.year + 2000 - 1900;
    time.tm_mon = t.month - 1;
    time.tm_mday = t.day;
    time.tm_hour = t.hour;
    time.tm_min = t.minute;
    time.tm_sec = t.second;

    // fix timezone
    *ts = mktime(&time) /* - (8 * 3600) */;
    return ESP_OK;
}

esp_err_t
max31328_load_alarm1(uint8_t *en, uint8_t *week_mode, uint8_t *af, uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *day_week) {
    max31328_init();

    uint8_t read_buf[4];
    esp_err_t err = i2c_read_reg(ADDR_SEC_ALARM, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    //ESP_LOGI(TAG, "read alarm raw: %x %x %x", read_buf[0], read_buf[1], read_buf[2]);

    clrbit(read_buf[0], 7);
    *second = bcd2hex(read_buf[0]);

    clrbit(read_buf[1], 7);
    *minute = bcd2hex(read_buf[1]);

    clrbit(read_buf[2], 7);
    *hour = bcd2hex(read_buf[2]);

    uint8_t saved_day_week = read_buf[3];
    *week_mode = (saved_day_week >> 6) & 0x01;
    if (*week_mode) {
        // week mode
        *day_week = saved_day_week & 0x0f;
    } else {
        bool day_ae = ((saved_day_week & 0b10000000) > 0);
        // day mode
        *day_week = bcd2hex(saved_day_week & 0x00111111);
        if (day_ae) {
            *day_week = *day_week | 0x01 << 7;
        }
    }

    err = max31328_read_alarm_status(en, af, NULL, NULL);
    if (err != ESP_OK) {
        return err;
    }

    return err;
}

esp_err_t max31328_set_alarm1(uint8_t en, uint8_t week_mode, uint8_t second, uint8_t minute, uint8_t hour, uint8_t day_week) {
    max31328_init();

    uint8_t write_day_week = hex2bcd(day_week);
    if (week_mode) {
        setbit(write_day_week, 6);
    }

    uint8_t write_buf[] = {ADDR_SEC_ALARM, hex2bcd(second),hex2bcd(minute), hex2bcd(hour), write_day_week};
    esp_err_t err = i2c_write(write_buf, sizeof(write_buf));
    if (err != ESP_OK) {
        return err;
    }

    uint8_t read_buf[2];
    err = i2c_read_reg(ADDR_CONTROL, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    // clear current alarm flag
    clrbit(read_buf[1], 0);

    if (en) {
        setbit(read_buf[0], 0);
        setbit(read_buf[0], 2);
    } else {
        clrbit(read_buf[0], 0);
    }

    write_buf[0] = ADDR_CONTROL;
    write_buf[1] = read_buf[0];
    write_buf[2] = read_buf[1];
    return i2c_write(write_buf, sizeof(write_buf));
}

esp_err_t max31328_set_alarm_en(uint8_t en1, uint8_t en2) {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_CONTROL, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    if (en1) {
        setbit(read_buf[0], 0);
    } else {
        clrbit(read_buf[0], 0);
    }

    if (en2) {
        setbit(read_buf[0], 1);
    } else {
        clrbit(read_buf[0], 1);
    }

    return i2c_write_byte(ADDR_CONTROL, read_buf[0]);
}

esp_err_t max31328_read_alarm_status(uint8_t *en1, uint8_t *flag1, uint8_t *en2, uint8_t *flag2) {
    uint8_t read_buf[2];
    esp_err_t err = i2c_read_reg(ADDR_CONTROL, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }
    *en1 = read_buf[0] & 0x01;
    *flag1 = read_buf[1] & 0x01;

    *en2 = (read_buf[0] >> 1) & 0x01;
    *flag2 = (read_buf[1] >> 1) & 0x01;

    return ESP_OK;
}

esp_err_t max31328_clear_alarm_flags(uint8_t alarm1, uint8_t alarm2) {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_STATUS, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    if (alarm1) {
        clrbit(read_buf[0], 0);
    }

    if (alarm2) {
        clrbit(read_buf[0], 1);
    }
    return i2c_write_byte(ADDR_STATUS, read_buf[0]);
}

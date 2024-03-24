//
// Created by yang on 2023/12/18.
//
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rx8025t.h"
#include "ws2812.h"
#include "beep/beep.h"
#include "key.h"
#include "LIS3DH.h"
#include "esp_types.h"
#include "esp_event.h"

#define I2C_MASTER_NUM              0
#define I2C_MASTER_TIMEOUT_MS       200

#define RX8025T_ADDR                 0b0110010

#define ADDR_SEC    0x00
#define ADDR_MIN    0x01
#define ADDR_HOUR   0x02
#define ADDR_WEEK   0x03
#define ADDR_DAY    0x04
#define ADDR_MONTH    0x05
#define ADDR_YEAR    0x06

#define ADDR_RAM    0x07

#define ADDR_MIN_ALARM    0x08
#define ADDR_HOUR_ALARM    0x09

#define ADDR_WEEK_ALARM    0x0A
#define ADDR_DAY_ALARM    0x0A

#define ADDR_TIMER_COUNTER_0    0x0B
#define ADDR_TIMER_COUNTER_1    0x0C

// [7]TEST [6]WADA [5]USEL [4]TE [3]FSEL1 [2]FSEL0 [1]TSEL1 [0]TSEL0
#define ADDR_EXT    0x0D

// [5]UF [4]TF [3]AF  [1]VLF [0]VDET
#define ADDR_FLAG    0x0E

// [7]CSEL1 [6]CSEL0 [5]UIE [4]TIE [3]AIE  [0]RESET
#define ADDR_CONTROL    0x0F

ESP_EVENT_DEFINE_BASE(BIKE_DATE_TIME_SENSOR_EVENT);

#define TAG "rx8025t"

extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;
static bool rx8025t_inited = false;

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

// bit0 sunday = 0
// bit6 Saturday = 6
static uint8_t transform_to_rx_week(uint8_t x) {
    if (x == 0) {
        return 0x01;
    }
    return 0x01 << x;
}

static uint8_t transform_from_rx_week(uint8_t x) {
    x = x >> 1;
    uint8_t w = 0;
    while (x) {
        x = x >> 1;
        w++;
    }

    return w;
}

static esp_err_t i2c_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev_handle, &reg_addr,
                                       1, data, len,
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t i2c_write_byte(uint8_t reg_addr, uint8_t data) {
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    return ret;
}

static esp_err_t i2c_write(const uint8_t *write_buffer, size_t write_size) {
    int ret;
    ret = i2c_master_transmit(dev_handle, write_buffer, write_size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

static void IRAM_ATTR rx8025_gpio_isr_handler(void *arg) {
    vTaskGenericNotifyGiveFromISR(tsk_hdl, 0, NULL);
}

static void config_intr_gpio() {
    gpio_config_t io_config = {
            .pin_bit_mask = (1ull << RX8025_INT_GPIO_NUM),
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_NEGEDGE,
            .pull_up_en = 0,
            .pull_down_en = 0,
    };

    ESP_ERROR_CHECK(gpio_config(&io_config));

    //install gpio isr service
    //gpio_install_isr_service(0);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(RX8025_INT_GPIO_NUM, rx8025_gpio_isr_handler, (void *) RX8025_INT_GPIO_NUM);
    ESP_LOGI(TAG, "rx8025t gpio isr add OK");
}

static void stop_alarm_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    stop_beep();
}

static void rx8025t_task_entry(void *arg) {
    // wait display ok
    // vTaskDelay(pdMS_TO_TICKS(300));
    config_intr_gpio();

    while (true) {
        // read af
        uint8_t uf = 0, tf = 0, af = 0;
        rx8025_read_flags(&uf, &tf, &af);

        uint8_t uie = 0, tie = 0, aie = 0;
        rx8025_read_ctrl(&uie, &tie, &aie);

        ESP_LOGI(TAG, "flags uf:%d tf:%d af:%d, uie:%d tie:%d aie:%d", uf, tf, af, uie, tie, aie);

        // int_gpio_level == 0
        // isr happens
        gpio_intr_disable(RX8025_INT_GPIO_NUM);

        if (gpio_get_level(RX8025_INT_GPIO_NUM) == 0) {
            ESP_LOGI(TAG, "isr happens gpio %d goes low", RX8025_INT_GPIO_NUM);
        }

        // clear af
        if (uf || tf || af) {
            rx8025_clear_flags(uf, tf, af);
        }

        if (af && aie) {
            ESP_LOGI(TAG, "== af isr happens ==");
            // read time check alarm is valid
            esp_event_handler_register(BIKE_KEY_EVENT, ESP_EVENT_ANY_ID, stop_alarm_handler,
                                            NULL);
            esp_event_handler_register(BIKE_MOTION_EVENT, ESP_EVENT_ANY_ID,
                                            stop_alarm_handler, NULL);

            common_post_event(BIKE_DATE_TIME_SENSOR_EVENT, RX8025T_SENSOR_ALARM_INTR);

            ESP_LOGI(TAG, "start show alarm color...");
            ws2812_init();
            ws2812_enable(true);
            ws2812_show_color_seq(ws2812_color_rgb_blink_500,
                                       sizeof(ws2812_color_rgb_blink_500) / sizeof(ws2812_color_rgb_blink_500[0]));
            ws2812_deinit();
            ESP_LOGI(TAG, "start show alarm color done!");

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

        if (uf && uie) {
            ESP_LOGI(TAG, "== uf isr happens ==");
            // update time flag
            common_post_event(BIKE_DATE_TIME_SENSOR_EVENT, RX8025T_SENSOR_UPDATE);
        }

        if (tf && tie) {
            ESP_LOGI(TAG, "== tf isr happens ==");
            // fix time flag
            common_post_event(BIKE_DATE_TIME_SENSOR_EVENT, RX8025T_SENSOR_FIX_TIME_INTR);
        }

        // clear all holding notification
        ulTaskGenericNotifyTake(0, pdTRUE, 0);
        gpio_intr_enable(RX8025_INT_GPIO_NUM);

        uint32_t notify_value = ulTaskGenericNotifyTake(0, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "get notify value %ld", notify_value);
    }

    vTaskDelete(NULL);
}

esp_err_t rx8025t_init() {
    if (rx8025t_inited) {
        return ESP_OK;
    }

    setenv("TZ", "CST-8", 1);
    tzset();

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = RX8025T_ADDR,
            .scl_speed_hz = 100000,
    };

    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t create_task_err = xTaskCreate(
            rx8025t_task_entry,
            "rx8025t_task",
            3072,
            NULL,
            0,
            &tsk_hdl);
    if (create_task_err != pdTRUE) {
        ESP_LOGE(TAG, "create rx8025 alarm detect task failed");
    }

    rx8025t_inited = true;
    ESP_LOGI(TAG, "init success!");
    return ESP_OK;
}

esp_err_t rx8025t_deinit() {
    if (!rx8025t_inited) {
        return ESP_OK;
    }

    vTaskDelete(tsk_hdl);
    gpio_isr_handler_remove(RX8025_INT_GPIO_NUM);

    rx8025t_inited = false;
    ESP_LOGI(TAG, "deinit success!");
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t
rx8025t_set_time(uint8_t year, uint8_t month, uint8_t day, uint8_t week, uint8_t hour, uint8_t minute, uint8_t second) {
    rx8025t_init();

    year = year % 2000;
    // year 0-99
    // month 1-12
    // day 1 - 31
    // week bit0 Sunday  bt6 Saturday
    // hour 0-23 minute 0-59 second 0-59

    uint8_t write_buf[] = {ADDR_SEC, hex2bcd(second), hex2bcd(minute), hex2bcd(hour),
                           transform_to_rx_week(week),
                           hex2bcd(day), hex2bcd(month), hex2bcd(year)};

    return i2c_write(write_buf, sizeof(write_buf));
}

esp_err_t rx8025t_get_time(uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *week, uint8_t *hour, uint8_t *minute,
                           uint8_t *second) {
    rx8025t_init();

    uint8_t read_buf[7];
    esp_err_t err = i2c_read_reg(ADDR_SEC, read_buf, sizeof(read_buf));
    if (err == ESP_OK) {
        *year = bcd2hex(read_buf[6]);
        *month = bcd2hex(read_buf[5]);
        *day = bcd2hex(read_buf[4]);
        *week = transform_from_rx_week(read_buf[3]);
        *hour = bcd2hex(read_buf[2]);
        *minute = bcd2hex(read_buf[1]);
        *second = bcd2hex(read_buf[0]);
    }

    return err;
}

esp_err_t rx8025t_get_time_ts(time_t *ts) {
    rx8025t_init();
    rx8025_time_t t;
    esp_err_t err = rx8025t_get_time(&t.year, &t.month, &t.day, &t.week, &t.hour, &t.minute, &t.second);
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

esp_err_t rx8025t_set_update_time_intr(uint8_t en, uint8_t per_minute) {
    rx8025t_init();

    uint8_t read_buf[3];
    esp_err_t err = i2c_read_reg(ADDR_EXT, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    // USEL
    if (per_minute) {
        // per minute
        setbit(read_buf[0], 5);
    } else {
        clrbit(read_buf[0], 5);
    }

    // UF
    clrbit(read_buf[1], 5);

    // UIE
    if (en) {
        setbit(read_buf[2], 5);
    } else {
        clrbit(read_buf[2], 5);
    }

    uint8_t write_buf[] = {ADDR_EXT, read_buf[0], read_buf[1], read_buf[2]};
    err = i2c_write(write_buf, sizeof(write_buf));
    return err;
}

esp_err_t rx8025t_read_update_time_intr(uint8_t *en, uint8_t *per_minute, uint8_t *flag) {
    rx8025t_init();

    uint8_t read_buf[3];
    esp_err_t err = i2c_read_reg(ADDR_EXT, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    *en = (read_buf[2] >> 5) & 0x01;
    *flag = (read_buf[1] >> 5) & 0x01;
    *per_minute = (read_buf[0] >> 5) & 0x01;

    return err;
}

esp_err_t rx8025t_clear_update_time_intr_flag() {
    return rx8025_clear_flags(1, 0, 0);
}

esp_err_t rx8025t_set_fixed_time_intr(uint8_t en, uint8_t intr_en, uint16_t total_ts) {
    rx8025t_init();

    uint8_t read_buf[3];
    esp_err_t err = i2c_read_reg(ADDR_EXT, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    bool second_mode = total_ts <= 4095;
    if (en) {
        setbit(read_buf[0], 4);
    } else {
        clrbit(read_buf[0], 4);
    }

    // mode
    if (second_mode) {
        // 1 0
        setbit(read_buf[0], 1);
        clrbit(read_buf[0], 0);
    } else {
        // minute mode
        setbit(read_buf[0], 1);
        clrbit(read_buf[0], 1);
    }

    // clear flag
    clrbit(read_buf[1], 4);

    // set intr
    if (intr_en) {
        setbit(read_buf[2], 4);
    } else {
        clrbit(read_buf[2], 4);
    }

    // write
    uint8_t write_time_low = second_mode ? total_ts & 0xff : (total_ts / 60) & 0xff;
    uint8_t write_time_high = second_mode ? (total_ts >> 8) & 0x0f : ((total_ts / 60) >> 8) & 0x0f;

    uint8_t write_buf[] = {ADDR_TIMER_COUNTER_0, write_time_low, write_time_high, read_buf[0], read_buf[1],
                           read_buf[2]};
    err = i2c_write(write_buf, sizeof(write_buf));

    ESP_LOGI(TAG, "set fix time intr en:%d intr_en:%d time %ds res:%d", en, intr_en, total_ts, err);
    return err;
}

esp_err_t rx8025t_read_fixed_time_intr(uint8_t *en, uint8_t *intr_en, uint16_t *total_ts, uint8_t *flag) {
    rx8025t_init();

    uint8_t read_buf[5];
    esp_err_t err = i2c_read_reg(ADDR_TIMER_COUNTER_0, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    uint16_t time_counter = ((read_buf[1] & 0x0f) << 8) | read_buf[0];
    uint8_t mode = read_buf[2] & 0b00000011;
    // 0  Once per 244.14 µs
    // 1  Once per 15.625 ms
    // 2   Once per second
    // 3   Once per minute
    switch (mode) {
        case 0:
            *total_ts = (uint16_t) ((float) time_counter / 1000 * 244.14) / 1000;
            break;
        case 1:
            *total_ts = (uint16_t) ((float) time_counter * 15.625) / 1000;
            break;
        case 2:
            *total_ts = time_counter;
            break;
        case 3:
            *total_ts = time_counter * 60;
            break;
        default:
            return ESP_FAIL;
    }

    *en = (read_buf[2] >> 4) & 0x01;
    *flag = (read_buf[3] >> 4) & 0x01;
    *intr_en = (read_buf[4] >> 4) & 0x01;

    return err;
}

esp_err_t rx8025t_clear_fixed_time_intr_flag() {
    return rx8025_clear_flags(0, 1, 0);
}

esp_err_t
rx8025_load_alarm(rx8025t_alarm_t *alarm) {
    rx8025t_init();

    uint8_t read_buf[3];
    esp_err_t err = i2c_read_reg(ADDR_MIN_ALARM, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    //ESP_LOGI(TAG, "read alarm raw: %x %x %x", read_buf[0], read_buf[1], read_buf[2]);

    // not support every minute mode
    clrbit(read_buf[0], 7);
    alarm->minute = bcd2hex(read_buf[0]);

    clrbit(read_buf[1], 7);
    clrbit(read_buf[1], 6);
    alarm->hour = bcd2hex(read_buf[1]);

    uint8_t saved_day_week = read_buf[2];

    err = i2c_read_reg(ADDR_EXT, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    // 0 week, 1 day
    alarm->mode = (read_buf[0] >> 6) & 0x01;
    alarm->af = (read_buf[1] >> 3) & 0x01;
    alarm->en = (read_buf[2] >> 3) & 0x01; // ADDR_CONTROL

    if (alarm->mode) {
        bool day_ae = ((saved_day_week & 0b10000000) > 0);
        // day mode
        alarm->day_week = bcd2hex(saved_day_week & 0x00111111);
        if (day_ae) {
            alarm->day_week = alarm->day_week | 0x01 << 7;
        }
    } else {
        // week mode
        alarm->day_week = saved_day_week;
    }

    return err;
}

esp_err_t rx8025_set_alarm(rx8025t_alarm_t *alarm) {
    rx8025t_init();

    uint8_t day_week = alarm->day_week;
    if (alarm->mode) {
        // day mode
        day_week = hex2bcd(day_week & 0x00111111) | (day_week & 0b10000000);
    }

    uint8_t write_buf[] = {ADDR_MIN_ALARM, hex2bcd(alarm->minute), hex2bcd(alarm->hour), day_week};
    esp_err_t err = i2c_write(write_buf, sizeof(write_buf));
    if (err != ESP_OK) {
        return err;
    }

    uint8_t read_buf[3];
    err = i2c_read_reg(ADDR_EXT, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    if (alarm->mode) {
        setbit(read_buf[0], 6);
    } else {
        clrbit(read_buf[0], 6);
    }

    // clear current alarm flag
    clrbit(read_buf[1], 3);

    if (alarm->en) {
        setbit(read_buf[2], 3);
    } else {
        clrbit(read_buf[2], 3);
    }

    write_buf[0] = ADDR_EXT;
    write_buf[1] = read_buf[0];
    write_buf[2] = read_buf[1];
    write_buf[3] = read_buf[2];

    ESP_LOGI(TAG, "set alarm en:%d mode:%d minute:%d, hour:%d dayweek:%d",
             alarm->en, alarm->mode, alarm->minute, alarm->hour, alarm->day_week);
    return i2c_write(write_buf, sizeof(write_buf));
}

esp_err_t rx8025_set_alarm_en(uint8_t en) {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_CONTROL, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    if (en) {
        setbit(read_buf[0], 3);
    } else {
        clrbit(read_buf[0], 3);
    }

    return i2c_write_byte(ADDR_CONTROL, read_buf[0]);
}

esp_err_t rx8025_read_alarm_flag(uint8_t *flag) {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_FLAG, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    *flag = (read_buf[0] >> 3) & 0x01;
    return ESP_OK;
}

esp_err_t rx8025_clear_alarm_flag() {
    return rx8025_clear_flags(0, 0, 1);
}

esp_err_t rx8025_read_flags(uint8_t *uf, uint8_t *tf, uint8_t *af) {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_FLAG, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    *af = (read_buf[0] >> 3) & 0x01;
    *tf = (read_buf[0] >> 4) & 0x01;
    *uf = (read_buf[0] >> 5) & 0x01;
    return ESP_OK;
}

esp_err_t rx8025_clear_flags(uint8_t uf, uint8_t tf, uint8_t af) {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_FLAG, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    if (af) {
        clrbit(read_buf[0], 3);
    }

    if (tf) {
        clrbit(read_buf[0], 4);
    }

    if (uf) {
        clrbit(read_buf[0], 5);
    }
    return i2c_write_byte(ADDR_FLAG, read_buf[0]);
}

esp_err_t rx8025_read_ctrl(uint8_t *uie, uint8_t *tie, uint8_t *aie) {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_CONTROL, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    *aie = (read_buf[0] >> 3) & 0x01;
    *tie = (read_buf[0] >> 4) & 0x01;
    *uie = (read_buf[0] >> 5) & 0x01;
    return ESP_OK;
}


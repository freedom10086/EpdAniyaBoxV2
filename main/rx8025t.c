//
// Created by yang on 2023/12/18.
//
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rx8025t.h"

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

static void IRAM_ATTR gpio_isr_handler(void *arg) {

}

void config_intr_gpio() {
    gpio_config_t io_config = {
            .pin_bit_mask = (1ull << RX8025_INT_GPIO_NUM),
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_NEGEDGE,
            .pull_up_en = 0,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    ESP_LOGI(TAG, "rx8025t int io %d, level %d", RX8025_INT_GPIO_NUM, gpio_get_level(RX8025_INT_GPIO_NUM));

    //install gpio isr service
    gpio_install_isr_service(0);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(RX8025_INT_GPIO_NUM, gpio_isr_handler, (void *) RX8025_INT_GPIO_NUM);
    ESP_LOGI(TAG, "rx8025t gpio isr add OK");
}

esp_err_t rx8025t_init() {
    if (rx8025t_inited) {
        return ESP_OK;
    }

    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = RX8025T_ADDR,
            .scl_speed_hz = 100000,
    };

    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        return err;
    }

    config_intr_gpio();

    rx8025t_inited = true;
    return ESP_OK;
}

esp_err_t rx8025t_deinit() {
    rx8025t_inited = false;
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t
rx8025t_set_time(uint8_t year, uint8_t month, uint8_t day, uint8_t week, uint8_t hour, uint8_t minute, uint8_t second) {
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

esp_err_t rx8025t_set_update_time_intr(uint8_t en, uint8_t per_minute) {
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
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_FLAG, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    clrbit(read_buf[0], 5);

    return i2c_write_byte(ADDR_FLAG, read_buf[0]);
}

esp_err_t rx8025t_set_fixed_time_intr(uint8_t en, uint8_t intr_en, uint16_t total_ts) {
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
    return err;
}

esp_err_t rx8025t_read_fixed_time_intr(uint8_t *en, uint8_t *intr_en, uint16_t *total_ts, uint8_t *flag) {
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
        case 1:
            *total_ts = (uint16_t) ((float) time_counter * 15.625) / 1000;
        case 2:
            *total_ts = time_counter;
        case 3:
            *total_ts = time_counter * 60;
    }

    *en = (read_buf[2] >> 4) & 0x01;
    *flag = (read_buf[3] >> 4) & 0x01;
    *intr_en = (read_buf[4] >> 4) & 0x01;

    return err;
}

esp_err_t rx8025t_clear_fixed_time_intr_flag() {
    uint8_t read_buf[1];
    esp_err_t err = i2c_read_reg(ADDR_FLAG, read_buf, sizeof(read_buf));
    if (err != ESP_OK) {
        return err;
    }

    clrbit(read_buf[0], 4);

    return i2c_write_byte(ADDR_FLAG, read_buf[0]);
}
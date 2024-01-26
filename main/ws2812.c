//
// Created by yang on 2023/12/19.
//
#include <string.h>
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "ws2812.h"
#include "ws2812_encoder.h"

#define TAG "ws2812"

#define RMT_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

static uint8_t led_strip_pixels[3];

rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;

static bool inited = false;
static bool on_off = false;
static bool rmt_enabled = false;

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b) {
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
        case 0:
            *r = rgb_max;
            *g = rgb_min + rgb_adj;
            *b = rgb_min;
            break;
        case 1:
            *r = rgb_max - rgb_adj;
            *g = rgb_max;
            *b = rgb_min;
            break;
        case 2:
            *r = rgb_min;
            *g = rgb_max;
            *b = rgb_min + rgb_adj;
            break;
        case 3:
            *r = rgb_min;
            *g = rgb_max - rgb_adj;
            *b = rgb_max;
            break;
        case 4:
            *r = rgb_min + rgb_adj;
            *g = rgb_min;
            *b = rgb_max;
            break;
        default:
            *r = rgb_max;
            *g = rgb_min;
            *b = rgb_max - rgb_adj;
            break;
    }
}

static void init_power_gpio() {
    uint64_t bit_mask = 1ull << LED_PWR_GPIO_NUM;
    gpio_config_t io_config = {
            .pin_bit_mask = bit_mask,
            .mode = GPIO_MODE_OUTPUT,
            .intr_type = GPIO_INTR_DISABLE,
            .pull_up_en = 0,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    on_off = gpio_get_level(LED_PWR_GPIO_NUM);
}

// onoff = 1 power on
void led_power_on_off(uint8_t onoff) {
    on_off = onoff;
    if (onoff) {
        ESP_LOGI(TAG, "ws2812 power on");
        gpio_set_level(LED_PWR_GPIO_NUM, 1);
    } else {
        ESP_LOGI(TAG, "ws2812 power off");
        gpio_set_level(LED_PWR_GPIO_NUM, 0);
    }
}

void ws2812_init() {
    if (inited) {
        return;
    }

    init_power_gpio();

    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
            .gpio_num = LED_DATA_GPIO_NUM,
            .mem_block_symbols = 128, // increase the block size can make the LED less flickering
            .resolution_hz = RMT_RESOLUTION_HZ,
            .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    led_strip_encoder_config_t encoder_config = {
            .resolution = RMT_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_ws2812_encoder(&encoder_config, &led_encoder));

    inited = true;
    ESP_LOGI(TAG, "inited onoff:%d", on_off);
}

esp_err_t ws2812_enable(bool enable) {
    if (!inited) {
        return ESP_FAIL;
    }

    led_power_on_off(enable);
    vTaskDelay(pdMS_TO_TICKS(5));

    esp_err_t err = ESP_OK;

    if (enable && !rmt_enabled) {
        err = rmt_enable(led_chan);
        rmt_enabled = true;
    }

    if (!enable && rmt_enabled) {
        err = rmt_disable(led_chan);
        rmt_enabled = false;
    }

    return err;
}

void ws2812_deinit() {
    if (!inited) {
        return;
    }

    ws2812_enable(false);
    inited = false;

    rmt_del_encoder(led_encoder);
    rmt_del_channel(led_chan);
}

static esp_err_t ws2812_show_color_inner(uint8_t r, uint8_t g, uint8_t b, uint16_t time_ms, bool add_shut_color) {
    led_strip_pixels[0] = g;
    led_strip_pixels[1] = b;
    led_strip_pixels[2] = r;

    rmt_transmit_config_t tx_config = {
            .loop_count = 0, // no transfer loop
    };

    esp_err_t err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);
    if (err != ESP_OK) {
        return err;
    }

    rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(time_ms));

    if (add_shut_color) {
        // shut down
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        err = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        if (err != ESP_OK) {
            return err;
        }

        rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    }

    return ESP_OK;
}

void ws2812_show_demo() {
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;

    ESP_LOGI(TAG, "Enable RMT TX channel");
    rmt_enable(led_chan);

    ESP_LOGI(TAG, "Start LED rainbow chase");
    rmt_transmit_config_t tx_config = {
            .loop_count = 0, // no transfer loop
    };

    // power on
    led_power_on_off(1);

    hue = start_rgb;

    for (int i = 0; i < 3; i++) {
        // Build RGB pixels
        led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
        ws2812_show_color_inner(red, green, blue, 500, false);
        ws2812_show_color_inner(0, 0, 0, 20, false);

        hue += 120;
    }

    // power off
    led_power_on_off(0);
}

esp_err_t ws2812_show_color(uint8_t r, uint8_t g, uint8_t b, uint16_t time_ms) {
    return ws2812_show_color_inner(r, g, b, time_ms, true);
}

esp_err_t ws2812_show_color_seq(const ws2812_color_item_t *color_seq, uint16_t swq_len) {
    esp_err_t err = ESP_OK;
    for (int i = 0; i < swq_len; ++i) {
        bool last = (i == (swq_len - 1));
        err = ws2812_show_color_inner(color_seq[i].rgb.r, color_seq[i].rgb.g, color_seq[i].rgb.b,
                                      color_seq[i].duration_ms,
                                      last);
        if (err != ESP_OK) {
            return err;
        }

    }
    return err;
}

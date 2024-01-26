//
// Created by yang on 2023/12/19.
//

#ifndef HELLO_WORLD_WS2812_H
#define HELLO_WORLD_WS2812_H

#define LED_DATA_GPIO_NUM       7
#define LED_PWR_GPIO_NUM        10

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_color_t;

typedef struct {
    ws2812_color_t rgb;
    uint32_t duration_ms;
} ws2812_color_item_t;

static const ws2812_color_item_t ws2812_color_red_blink_500[] = {
        {{255, 0, 0}, 500},
        {{0, 0, 0}, 500},
        {{255, 0, 0}, 500},
};

static const ws2812_color_item_t ws2812_color_green_blink_500[] = {
        {{0, 255, 0}, 500},
        {{0, 0, 0}, 500},
        {{0, 255, 0}, 500},
};

static const ws2812_color_item_t ws2812_color_blue_blink_500[] = {
        {{0, 0, 255}, 500},
        {{0, 0, 0}, 500},
        {{0, 0, 255}, 500},
};

static const ws2812_color_item_t ws2812_color_rgb_blink_500[] = {
        {{255, 0, 0}, 500},
        {{0, 0, 0}, 500},
        {{0, 255, 0}, 500},
        {{0, 0, 0}, 500},
        {{0, 0, 255}, 500},
        {{0, 0, 0}, 500},
};

void ws2812_init();

esp_err_t ws2812_enable(bool enable);

void ws2812_show_demo();

esp_err_t ws2812_show_color(uint8_t r, uint8_t g, uint8_t b, uint16_t time_ms);

esp_err_t ws2812_show_color_seq(const ws2812_color_item_t *color_seq, uint16_t swq_len);

void ws2812_deinit();

#endif //HELLO_WORLD_WS2812_H

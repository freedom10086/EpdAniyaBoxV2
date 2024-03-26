/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_H_RES 200
#define LCD_V_RES 200

#define DISP_CS_GPIO_NUM (-1)
#define DISP_DC_GPIO_NUM 22
#define DISP_RST_GPIO_NUM 25
#define DISP_BUSY_GPIO_NUM 10

typedef enum {
    EPD_REFRESH_MODE_UNSET = 0,
    EPD_REFRESH_MODE_FULL,
    EPD_REFRESH_MODE_PARTIAL
} epd_refresh_mode_t;

typedef struct {
    spi_device_handle_t spi_dev;
    int busy_gpio_num; /*! LOW: idle, HIGH: busy */
    int reset_gpio_num; /*!< GPIO used to reset the LCD panel, set to -1 if it's not used */
    int dc_gpio_num;
    bool reset_level;

    epd_refresh_mode_t refresh_mode;

    int _current_mem_area_start_x;
    int _current_mem_area_end_x;
    int _current_mem_area_start_y;
    int _current_mem_area_end_y;
    int _current_mem_pointer_x;
    int _current_mem_pointer_y;
} lcd_ssd1680_panel_t;

/**
 * @brief Create LCD panel for model GC9A01
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config general panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_OK                on success
 */
esp_err_t epd_panel_init(spi_host_device_t bus);

esp_err_t epd_panel_init_full();

esp_err_t epd_panel_init_partial();

esp_err_t epd_panel_reset();

esp_err_t epd_panel_clear_display(uint8_t color);

esp_err_t epd_panel_draw_bitmap(int16_t x_start, int16_t y_start, int16_t x_end, int16_t y_end,
                                           const void *color_data) ;

esp_err_t epd_panel_refresh(bool full_refresh, bool waitdone);

esp_err_t epd_panel_refresh_area(int16_t x, int16_t y, int16_t end_x, int16_t end_y, bool waitdone);

esp_err_t epd_panel_sleep();

esp_err_t epd_panel_del();


#ifdef __cplusplus
}
#endif
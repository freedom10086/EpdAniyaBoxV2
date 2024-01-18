#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "driver/spi_master.h"
#include "esp_sleep.h"

#include "common_utils.h"
#include "epd_lcd_ssd1680.h"
#include "epdpaint.h"
#include "key.h"
#include "LIS3DH.h"
#include "display.h"
#include "page_manager.h"
#include "box_common.h"
#include "bles/ble_server.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "display"

#define DISP_SPI_HOST SPI2_HOST

#define DISP_BUFF_SIZE LCD_H_RES * LCD_V_RES

ESP_EVENT_DEFINE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void guiTask(void *pvParameter);

static TaskHandle_t x_update_notify_handl = NULL;
static uint32_t boot_cnt = 0;
static uint32_t lst_event_tick;
static bool updating = false;
static bool rotation_change = false;
static uint8_t curr_disp_rotation;

extern esp_event_loop_handle_t event_loop_handle;

static void register_event_callbacks();

uint8_t calc_disp_rotation(uint8_t default_rotate) {
    lis3dh_direction_t disp_direction = lis3dh_get_direction();
    switch (disp_direction) {
        case LIS3DH_DIR_LEFT:
            return ROTATE_270;
        case LIS3DH_DIR_BOTTOM:
            return ROTATE_180;
        case LIS3DH_DIR_RIGHT:
            return ROTATE_90;
        case LIS3DH_DIR_TOP:
            return ROTATE_0;
        default:
            return default_rotate;
    }
}

bool spi_driver_init(int host,
                     int miso_pin, int mosi_pin, int sclk_pin,
                     int max_transfer_sz,
                     int dma_channel) {

    ESP_LOGI(TAG, "Initialize SPI bus");

    assert((0 <= host));
    const char *spi_names[] = {
            "SPI1_HOST", "SPI2_HOST", "SPI3_HOST"
    };

    ESP_LOGI(TAG, "Configuring SPI host %s [%d]", spi_names[host], host);
    ESP_LOGI(TAG, "MISO pin: %d, MOSI pin: %d, SCLK pin: %d", miso_pin, mosi_pin, sclk_pin);

    ESP_LOGI(TAG, "Max transfer size: %d (bytes)", max_transfer_sz);

    spi_bus_config_t buscfg = {
            .miso_io_num = miso_pin,
            .mosi_io_num = mosi_pin,
            .sclk_io_num = sclk_pin,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = max_transfer_sz
    };

    ESP_LOGI(TAG, "Initializing SPI bus...");
    ESP_LOGI(TAG, "SPI DMA channel %d", dma_channel);
    esp_err_t ret = spi_bus_initialize(host, &buscfg, (spi_dma_chan_t) dma_channel);
    assert(ret == ESP_OK);

    return ESP_OK != ret;
}

void draw_page(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    page_inst_t current_page = page_manager_get_current_page();
    current_page.on_draw_page(epd_paint, loop_cnt);

    if (page_manager_has_menu()) {
        page_inst_t current_menu = page_manager_get_current_menu();
        current_menu.on_draw_page(epd_paint, loop_cnt);
    }
}

void after_draw_page(uint32_t loop_cnt) {
    page_inst_t current_page = page_manager_get_current_page();
    if (current_page.after_draw_page != NULL) {
        current_page.after_draw_page(loop_cnt);
    }

    if (page_manager_has_menu()) {
        page_inst_t menu = page_manager_get_current_menu();
        if (menu.after_draw_page != NULL) {
            menu.after_draw_page(loop_cnt);
        }
    }
}

static void guiTask(void *pvParameter) {
    int8_t page_index = page_manager_get_current_index();
    if (page_index >= 0 && page_index < HOME_PAGE_COUNT) {
        page_manager_init(page_index);
    } else {
        page_manager_init(TEMP_PAGE_INDEX);
    }

    x_update_notify_handl = xTaskGetCurrentTaskHandle();

    spi_driver_init(DISP_SPI_HOST,
                    DISP_MISO_GPIO_NUM, DISP_MOSI_GPIO_NUM, DISP_CLK_GPIO_NUM,
                    DISP_BUFF_SIZE, SPI_DMA_CH_AUTO);

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num = DISP_DC_GPIO_NUM,
            .cs_gpio_num = DISP_CS_GPIO_NUM,
            .pclk_hz = 10 * 1000 * 1000, // 10M
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .spi_mode = 0,
            //.on_color_trans_done = ,
    };

    lcd_ssd1680_panel_t panel = {
            .busy_gpio_num = DISP_BUSY_GPIO_NUM,
            .reset_gpio_num = DISP_RST_GPIO_NUM,
            .reset_level = 0,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(new_panel_ssd1680(&panel, DISP_SPI_HOST, &io_config));

    ESP_LOGI(TAG, "Reset SSD1680 panel driver");
    panel_ssd1680_reset(&panel);

    // for test
    epd_paint_t *epd_paint = malloc(sizeof(epd_paint_t));

    //uint8_t *image = malloc(sizeof(uint8_t) * LCD_H_RES * LCD_V_RES / 8);
    uint8_t *image = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint8_t) / 8, MALLOC_CAP_DMA);
    if (!image) {
        ESP_LOGE(TAG, "no memory for display driver");
        return;
    }

    uint8_t rotation = calc_disp_rotation(DEFAULT_DISP_ROTATION);
    curr_disp_rotation = rotation;
    epd_paint_init(epd_paint, image, LCD_H_RES, LCD_V_RES, rotation);
    epd_paint_clear(epd_paint, 0);

    panel_ssd1680_init_full(&panel);

    static uint32_t loop_cnt = 1;
    uint32_t last_full_refresh_loop_cnt = loop_cnt;
    static uint32_t current_tick, next_check_display_timeout_tick;
    static uint32_t ulNotificationCount;
    bool wakeup_by_timer = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);

    //sleep wait for sensor init
    vTaskDelay(pdMS_TO_TICKS(10));

    register_event_callbacks();

    while (1) {
        // not first loop
        if (loop_cnt > 1 && !wakeup_by_timer) {
            // ulNotificationCount > 0 success get or timeout
            ulNotificationCount = ulTaskGenericNotifyTake(0, pdTRUE, pdMS_TO_TICKS(5000));
            // ESP_LOGI(TAG, "ulTaskGenericNotifyTake %ld", ulNotificationCount);
        }

        current_tick = xTaskGetTickCount();
        bool display_timeout = (current_tick >= next_check_display_timeout_tick)
                               && pdTICKS_TO_MS(current_tick - lst_event_tick) >= DEEP_SLEEP_TIMEOUT_MS;
        if (display_timeout) {
            next_check_display_timeout_tick = current_tick + pdMS_TO_TICKS(DEEP_SLEEP_TIMEOUT_MS);
        }

        bool will_enter_deep_sleep = display_timeout || wakeup_by_timer;
        if (ulNotificationCount > 0 || loop_cnt == 1 || display_timeout) {
            ESP_LOGI(TAG, "draw loop: %ld, boot_cnt: %ld  ulNotification: %ld lst_full_loop:%ld", loop_cnt, boot_cnt,
                     ulNotificationCount, last_full_refresh_loop_cnt);

            // use partial update mode
            // less continue 60 times partial refresh mode or last full update time less 30min and not first loop
            // if will enter deep sleep mode use full update
            bool use_full_update_mode = loop_cnt == 1
                                        || loop_cnt - last_full_refresh_loop_cnt >= 60
                                        || will_enter_deep_sleep;
            bool use_partial_update_mode = !use_full_update_mode;

            if (panel._using_partial_mode != use_partial_update_mode) {
                if (use_partial_update_mode) {
                    panel_ssd1680_init_partial(&panel);
                } else {
                    panel_ssd1680_init_full(&panel);
                }
            }

            if (rotation_change) {
                epd_paint_set_rotation(epd_paint, curr_disp_rotation);
                rotation_change = false;
            }

            updating = true;
            // clear all holding update request
            ulTaskGenericNotifyTake(0, pdTRUE, 0);

            draw_page(epd_paint, loop_cnt);
            panel_ssd1680_draw_bitmap(&panel, 0, 0, LCD_H_RES, LCD_V_RES, epd_paint->image);
            panel_ssd1680_refresh(&panel, use_partial_update_mode);
            updating = false;
            after_draw_page(loop_cnt);

            if (use_full_update_mode) {
                last_full_refresh_loop_cnt = loop_cnt;
            }

            loop_cnt += 1;
        }

        // enter deep sleep mode
        if (display_timeout || wakeup_by_timer) {
            int sleepTs = page_manager_enter_sleep(loop_cnt);
            if (sleepTs >= 0) {
                ESP_LOGI(TAG, "%dms timeout enter sleep. sleep ts %d", DEEP_SLEEP_TIMEOUT_MS, sleepTs);
                panel_ssd1680_sleep(&panel);
                box_enter_deep_sleep(sleepTs);
            } else {
                // never sleep

            }
        }
    }

    panel_ssd1680_sleep(&panel);
    epd_paint_deinit(epd_paint);
    free(image);
    free(epd_paint);

    vTaskDelete(NULL);
}

void request_display_update_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data) {
    if (BIKE_REQUEST_UPDATE_DISPLAY_EVENT == event_base) {
        //ESP_LOGI(TAG, "request for update...");
        int full_update = (int) event_data;
        xTaskGenericNotify(x_update_notify_handl, 0, full_update,
                           eIncrement, NULL);
    }
}

static void acc_motion_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                     void *event_data) {
    switch (event_id) {
        case LIS3DH_DIRECTION_CHANGE:
            if (rotation_change) {
                return;
            }
            rotation_change = true;
            lis3dh_direction_t *d = (lis3dh_direction_t *) event_data;
            curr_disp_rotation = calc_disp_rotation(curr_disp_rotation);
            ESP_LOGI(TAG, "request update for rotation change %d %d", *d, curr_disp_rotation);
            page_manager_request_update(false);
            break;
        default:
            lst_event_tick = xTaskGetTickCount();
    }
}

static void update_lst_event_tick_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    lst_event_tick = xTaskGetTickCount();
}

static void register_event_callbacks() {
    // key click event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_KEY_EVENT, ESP_EVENT_ANY_ID,
                                    update_lst_event_tick_handler, NULL);

    // update display event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_REQUEST_UPDATE_DISPLAY_EVENT, ESP_EVENT_ANY_ID,
                                    request_display_update_handler, NULL);

    // lis3dsh event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_MOTION_EVENT, ESP_EVENT_ANY_ID,
                                    acc_motion_event_handler, NULL);

    // ble server event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_BLE_SERVER_EVENT, ESP_EVENT_ANY_ID,
                                    update_lst_event_tick_handler, NULL);
}

void display_init(uint32_t boot_count) {
    boot_cnt = boot_count;
    lst_event_tick = xTaskGetTickCount();

    // uxPriority 0 最低
    xTaskCreate(guiTask, "gui", 4096 * 2, NULL, 1, NULL);
}


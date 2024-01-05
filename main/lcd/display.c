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
#include "nvs.h"
#include "box_common.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "display"

#define TFT_SPI_HOST SPI2_HOST

#define DISP_BUFF_SIZE LCD_H_RES * LCD_V_RES
#define DISP_STORAGE_NAMESPACE "display"

ESP_EVENT_DEFINE_BASE(BIKE_REQUEST_UPDATE_DISPLAY_EVENT);

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void guiTask(void *pvParameter);

static TaskHandle_t x_update_notify_handl = NULL;
static uint32_t boot_cnt = 0;
static uint32_t lst_event_tick, lst_check_direction_tick;
bool updating = false;
bool holding_updating = false;
static uint8_t curr_disp_rotation;

extern esp_event_loop_handle_t event_loop_handle;

uint8_t get_saved_disp_rotation() {
    uint8_t rotation = DEFAULT_DISP_ROTATION;

    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(DISP_STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) return rotation;

    // Read
    err = nvs_get_u8(my_handle, "disp_rotation", &rotation);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return rotation;

    // Close
    nvs_close(my_handle);
    return rotation;
}

uint8_t calc_disp_rotation(uint8_t default_rotate) {
    lis3dh_direction_t disp_direction = lis3dh_calc_direction();
    ESP_LOGI(TAG, "calc display rotation %d", disp_direction);
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

esp_err_t save_disp_rotation(uint8_t rotation) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(DISP_STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Write
    err = nvs_set_u8(my_handle, "disp_rotation", rotation);
    if (err != ESP_OK) return err;

    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
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
    if (page_index == IMAGE_PAGE_INDEX) { // image page
        page_manager_init("image");
    } else {
        // TODO for debug
        //page_manager_init("date-time");
        page_manager_init("temperature");
    }

    x_update_notify_handl = xTaskGetCurrentTaskHandle();

    spi_driver_init(TFT_SPI_HOST,
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
    ESP_ERROR_CHECK(new_panel_ssd1680(&panel, TFT_SPI_HOST, &io_config));

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

    curr_disp_rotation = get_saved_disp_rotation();
    epd_paint_init(epd_paint, image, LCD_H_RES, LCD_V_RES, curr_disp_rotation);
    epd_paint_clear(epd_paint, 0);

    panel_ssd1680_init_full(&panel);

    static uint32_t loop_cnt = 1;
    uint32_t last_full_refresh_loop_cnt = loop_cnt;
    uint32_t last_full_refresh_tick = xTaskGetTickCount();
    static uint32_t current_tick;
    static uint32_t ulNotificationCount;
    static uint32_t continue_time_out_count = 0;
    bool wakeup_by_timer = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
    lst_check_direction_tick = 0;

    //sleep wait for sensor init
    vTaskDelay(pdMS_TO_TICKS(50));

    while (1) {
        // not first loop
        if (loop_cnt > 1 && !wakeup_by_timer && !holding_updating) {
            ulNotificationCount = ulTaskGenericNotifyTake(0, pdTRUE, pdMS_TO_TICKS(10000));
            // ESP_LOGI(TAG, "ulTaskGenericNotifyTake %ld", ulNotificationCount);
            if (ulNotificationCount > 0) { // may > 1 more data ws send
                continue_time_out_count = 0;
            } else {
                /* The call to ulTaskNotifyTake() timed out. */
                continue_time_out_count += 1;
            }
        }

        current_tick = xTaskGetTickCount();
        bool display_time_out = pdTICKS_TO_MS(current_tick - lst_event_tick) >= DEEP_SLEEP_TIMEOUT_MS;

        bool rotation_change = false;
        // 重新计算屏幕旋转角度 必须是静态情况下(5s 没有按键/加速度)
        if ((ulNotificationCount == 0
             && pdTICKS_TO_MS(current_tick - lst_check_direction_tick) >= 5000
             && pdTICKS_TO_MS(current_tick - lst_event_tick) >= 5000)
            || wakeup_by_timer) {
            uint8_t new_rotation = calc_disp_rotation(curr_disp_rotation);
            lst_check_direction_tick = current_tick;
            if (new_rotation != curr_disp_rotation) {
                ESP_LOGI(TAG, "disp rotation change from %d to %d", curr_disp_rotation, new_rotation);
                curr_disp_rotation = new_rotation;
                epd_paint_set_rotation(epd_paint, curr_disp_rotation);
                save_disp_rotation(curr_disp_rotation);
                rotation_change = true;
            }
        }

        bool will_enter_deep_sleep = display_time_out || wakeup_by_timer;
        if (ulNotificationCount > 0 || display_time_out || loop_cnt == 1 || rotation_change) {
            holding_updating = false;
            ESP_LOGI(TAG, "draw loop: %ld, boot_cnt: %ld  ulNotification: %ld", loop_cnt, boot_cnt,
                     ulNotificationCount);

            // use partial update mode
            // less continue 60 times partial refresh mode or last full update time less 30min and not first loop
            // if will enter deep sleep mode use full update
            bool use_full_update_mode = loop_cnt == 1
                                        || loop_cnt - last_full_refresh_loop_cnt >= 60
                                        || (will_enter_deep_sleep && pdTICKS_TO_MS(current_tick - last_full_refresh_tick) >= 5000);
            bool use_partial_update_mode = !use_full_update_mode;

            //if (display_time_out) {
            //    panel_ssd1680_clear_display(&panel, 0xff);
            //}

            if (panel._using_partial_mode != use_partial_update_mode) {
                if (use_partial_update_mode) {
                    panel_ssd1680_init_partial(&panel);
                } else {
                    panel_ssd1680_init_full(&panel);
                }
            }

            draw_page(epd_paint, loop_cnt);
            updating = true;
            panel_ssd1680_draw_bitmap(&panel, 0, 0, LCD_H_RES, LCD_V_RES, epd_paint->image);
            panel_ssd1680_refresh(&panel, use_partial_update_mode);
            updating = false;
            after_draw_page(loop_cnt);

            if (use_full_update_mode) {
                last_full_refresh_tick = current_tick;
                last_full_refresh_loop_cnt = loop_cnt;
            }

            loop_cnt += 1;
        }

        // enter deep sleep mode
        if (display_time_out || wakeup_by_timer) {
            int sleepTs = page_manager_enter_sleep(loop_cnt);
            if (sleepTs >= 0) {
                if (display_time_out) {
                    ESP_LOGI(TAG, "%dms timeout enter sleep. sleep ts %d", DEEP_SLEEP_TIMEOUT_MS, sleepTs);
                }

                panel_ssd1680_sleep(&panel);
                box_enter_deep_sleep(sleepTs);
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
        if (updating) {
            holding_updating = true;
            ESP_LOGI(TAG, "still updating hold...");
        } else {
            //ESP_LOGI(TAG, "request for update...");
            int *full_update = (int *) event_data;
            xTaskGenericNotify(x_update_notify_handl, 0, *full_update,
                               eIncrement, NULL);
        }
    }
}

static void key_click_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data) {
    lst_event_tick = xTaskGetTickCount();
    //ESP_LOGI(TAG, "rev key click event %ld", event_id);
    // if menu exist
    if (page_manager_has_menu()) {
        page_inst_t current_menu = page_manager_get_current_menu();
        if (current_menu.key_click_handler) {
            if (current_menu.key_click_handler(event_id)) {
                return;
            }
        }
    }

    // if not handle passed to view
    page_inst_t current_page = page_manager_get_current_page();
    if (current_page.key_click_handler) {
        if (current_page.key_click_handler(event_id)) {
            return;
        }
    }

    // finally pass here
    switch (event_id) {
        case KEY_UP_SHORT_CLICK:
            break;
        case KEY_DOWN_SHORT_CLICK:
            break;
        case KEY_CANCEL_SHORT_CLICK:
            if (page_manager_has_menu()) {
                page_manager_close_menu();
                page_manager_request_update(false);
                return;
            } else {
                page_manager_close_page();
                page_manager_request_update(false);
                return;
            }
            break;
        case KEY_OK_LONG_CLICK:
            if (page_manager_has_menu()) {
                page_manager_close_menu();
                page_manager_request_update(false);
                return;
            } else {
                page_manager_show_menu("menu");
                page_manager_request_update(false);
                return;
            }
            break;
        case KEY_UP_LONG_CLICK:
        case KEY_DOWN_LONG_CLICK:
            if (page_manager_get_current_index() == TEMP_PAGE_INDEX) {
                page_manager_switch_page("image", false);
                page_manager_request_update(false);
            } else {
                page_manager_switch_page("temperature", false);
                page_manager_request_update(false);
            }
        default:
            break;
    }

    // if page not handle key click event here handle
    ESP_LOGI(TAG, "no page handler key click event %ld", event_id);
}

static void acc_motion_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                     void *event_data) {
    lst_event_tick = xTaskGetTickCount();
}

void display_init(uint32_t boot_count) {
    boot_cnt = boot_count;
    lst_event_tick = xTaskGetTickCount();

    // uxPriority 0 最低
    xTaskCreate(guiTask, "gui", 4096 * 2, NULL, 1, NULL);

    // key click event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_KEY_EVENT, ESP_EVENT_ANY_ID,
                                    key_click_event_handler, NULL);

    // update display event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_REQUEST_UPDATE_DISPLAY_EVENT, ESP_EVENT_ANY_ID,
                                    request_display_update_handler, NULL);

    // lis3dsh event
    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_MOTION_EVENT, ESP_EVENT_ANY_ID,
                                    acc_motion_event_handler, NULL);
}


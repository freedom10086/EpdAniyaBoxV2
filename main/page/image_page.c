#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <esp_log.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "lcd/epdpaint.h"
#include "key.h"
#include "lcd/jpg.h"
#include "lcd/bmp.h"

#include "image_page.h"
#include "lcd/epd_lcd_ssd1680.h"
#include "static/static.h"
#include <dirent.h>
#include "esp_vfs.h"

#include "confirm_menu_page.h"
#include "alert_dialog_page.h"
#include "page_manager.h"
#include "bles/ble_server.h"
#include "wifi/my_file_server_common.h"
#include "battery.h"
#include "view/battery_view.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "bitmap-page"

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

RTC_DATA_ATTR static int16_t current_bitmap_page_index = 0;
RTC_DATA_ATTR static int16_t total_image_file_count = -1;

static char current_filepath[64];
bool file_system_mounted = false;

static void calc_total_image_file_count();

static void delete_menu_callback(bool confirm);

static esp_err_t delete_current_image();

static confirm_menu_arg_t confirm_menu_arg = {
        .title_label = "delete?",
        .callback = delete_menu_callback
};

static void delete_menu_callback(bool confirm) {
    ESP_LOGI(TAG, "delete_menu_callback:%d", confirm);
    if (!confirm) {
        return;
    }
    delete_current_image();
}

void testBmp(uint8_t *data, uint16_t data_len) {
    //bmp_header *bmpHeader = (bmp_header *) (aniya_200_1_bmp_start);
    bmp_header bmpHeader1;
    enum bmp_error err = bmp_header_read(&bmpHeader1, data, data_len);
    bmp_header *bmpHeader = &bmpHeader1;
    if (err != BMP_OK || bmpHeader->bfType != BMP_MAGIC) {
        ESP_LOGE(TAG, "not a bmp file %X %X, err: %d", ((uint16_t *) data)[0], bmpHeader->bfType, err);
    }

    ESP_LOGI(TAG,
             "bfType = 0x%X, bfSize = %ld, bfReserved1 = 0x%X, bfReserved2 = 0x%X, bfOffBits = %ld,"
             "biSize = %ld, biWidth = %ld, biHeight = %ld,"
             "biBitCount = %d, biCompression = %ld, biSizeImage = %ld,"
             "biClrUsed = %ld, biClrImportant = %ld",
             bmpHeader->bfType, bmpHeader->bfSize,
             bmpHeader->bfReserved1, bmpHeader->bfReserved2, bmpHeader->bfOffBits,
             bmpHeader->biSize, bmpHeader->biWidth, bmpHeader->biHeight,
             bmpHeader->biBitCount, bmpHeader->biCompression, bmpHeader->biSizeImage,
             bmpHeader->biClrUsed, bmpHeader->biClrImportant);

    if (bmpHeader->biBitCount != 1 && bmpHeader->biBitCount != 4
        && bmpHeader->biBitCount != 8 && bmpHeader->biBitCount != 16
        && bmpHeader->biBitCount != 24 && bmpHeader->biBitCount != 32) {
        ESP_LOGW(TAG, "not a supported bmp file biBitCount : %d", bmpHeader->biBitCount);
        return;
    }

    // stride = ((((biWidth * biBitCount) + 31) & ~31) >> 3)

    if ((bmpHeader->biBitCount == 1 || bmpHeader->biBitCount == 4 || bmpHeader->biBitCount == 8)
        && bmpHeader->biCompression == BI_RGB) {
        int bmiColorsCount = (bmpHeader->bfOffBits - 14 - bmpHeader->biSize) / sizeof(RGBQUAD);
        assert(bmiColorsCount == 1 << bmpHeader->biBitCount);
    } else if ((bmpHeader->biBitCount == 16 || bmpHeader->biBitCount == 32)
               && bmpHeader->biCompression == BI_BITFIELDS) {
        assert(bmpHeader->bfOffBits - 14 - bmpHeader->biSize == sizeof(RGBQUAD_COLOR_MASK));
    } else if ((bmpHeader->biBitCount == 24 || bmpHeader->biBitCount == 32) && bmpHeader->biCompression == BI_RGB) {
        // no color data
        assert(bmpHeader->bfSize - bmpHeader->bfOffBits == bmpHeader->biSizeImage);
    } else {
        ESP_LOGW(TAG, "not supported bmp file biBitCount : %d biCompression:%ld",
                 bmpHeader->biBitCount,
                 bmpHeader->biCompression);
        return;
    }

    // check data size
    uint16_t stride = ((((bmpHeader->biWidth * bmpHeader->biBitCount) + 31) & ~31) >> 3);
    uint16_t min_byte_count = stride * abs(bmpHeader->biHeight);
    if (bmpHeader->biSizeImage > 0 && bmpHeader->biCompression == BI_RGB) {
        assert(bmpHeader->biSizeImage >= min_byte_count);
    }

    uint8_t lut_count = 1 << bmpHeader->biBitCount;
    if (bmpHeader->biBitCount == 1 || bmpHeader->biBitCount == 4 || bmpHeader->biBitCount == 8) {
        RGBQUAD *color = (RGBQUAD *) (data + sizeof(bmp_header));
        for (int i = 0; i < lut_count; ++i) {
            ESP_LOGI(TAG, "lut : %d, blue:%d green:%d red:%d", i, color[i].blue, color[i].green, color[i].red);
        }
    }

    pixel_color color;
    bmp_img_common *bmp_img = (bmp_img_common *) data;
    bmp_get_pixel(&color, bmp_img, 0, 0);
    ESP_LOGI(TAG, "pixel : x:%d, y:%d blue:%d green:%d red:%d", 0, 0, color.blue, color.green, color.red);
}

void display_file(epd_paint_t *epd_paint, uint32_t loop_cnt, char *file_name, uint16_t file_size) {
    ESP_LOGI(TAG, "display image file %s", file_name);
    FILE *img_file = fopen(file_name, "r");
    if (img_file == NULL) {
        ESP_LOGE(TAG, "open image file %s failed", file_name);
        current_bitmap_page_index = 0;
        image_page_draw(epd_paint, loop_cnt);
        return;
    }

    strcpy(current_filepath, file_name);

    if (IS_FILE_EXT(file_name, ".bmp")) {
        epd_paint_draw_bitmap_file(epd_paint, 0, 0, LCD_H_RES, LCD_V_RES, img_file, 1);
        ESP_LOGI(TAG, "display bmp file %s finish", file_name);
    } else {
        epd_paint_draw_jpg_file(epd_paint, 0, 0, LCD_H_RES, LCD_V_RES, img_file, file_size, 1);
        ESP_LOGI(TAG, "display jpg file %s finish", file_name);
    }
    fclose(img_file);
}

void image_page_on_create(void *arg) {
    esp_err_t err = mount_storage(FILE_SERVER_BASE_PATH, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed %s", FILE_SERVER_BASE_PATH);
        file_system_mounted = false;
    } else {
        file_system_mounted = true;
    }
}

static bool
check_is_valid_image_file(struct dirent *entry, char *entrypath, size_t dirpath_len, struct stat *entry_stat) {
    if (entry->d_type != DT_REG) {
        return false;
    }

    strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
    if (stat(entrypath, entry_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat %s", entry->d_name);
        return false;;
    }
    static char entrysize[16];
    sprintf(entrysize, "%ld", (*entry_stat).st_size);
    ESP_LOGI(TAG, "Found %s (%s bytes)", entry->d_name, entrysize);

    if ((*entry_stat).st_size > MAX_FILE_SIZE) {
        return false;;
    }

    if (!IS_FILE_EXT(entry->d_name, ".bmp") && !IS_FILE_EXT(entry->d_name, ".jpg")) {
        return false;;
    }
    return true;
}

void image_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    if (current_bitmap_page_index == 0) {
        epd_paint_draw_bitmap(epd_paint, 0, 0, LCD_H_RES, LCD_V_RES, (uint8_t *) aniya_200_1_bmp_start,
                              aniya_200_1_bmp_end - aniya_200_1_bmp_start, 1);
    } else if (file_system_mounted) {
        // read from file
        bool finded = false;

        char entrypath[64];

        struct dirent *entry;
        struct stat entry_stat;

        /* Retrieve the base path of file storage to construct the full path */
        strcpy(entrypath, FILE_SERVER_BASE_PATH);
        strlcpy(entrypath + strlen(FILE_SERVER_BASE_PATH), "/", sizeof(entrypath) - strlen(FILE_SERVER_BASE_PATH));

        ESP_LOGI(TAG, "open dir %s", entrypath);
        DIR *dir = opendir(entrypath);
        const size_t dirpath_len = strlen(entrypath);

        if (!dir) {
            ESP_LOGE(TAG, "Failed to stat dir : %s", entrypath);
        } else {
            ESP_LOGI(TAG, "list dir : %s", entrypath);
            uint16_t curr_file_index = 1; // start from 1 because has one default image
            /* Iterate over all files / folders and fetch their names and sizes */
            while ((entry = readdir(dir)) != NULL) {
                if (!check_is_valid_image_file(entry, entrypath, dirpath_len, &entry_stat)) {
                    continue;
                }

                if (current_bitmap_page_index == curr_file_index) {
                    // finded
                    finded = true;
                    strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
                    display_file(epd_paint, loop_cnt, entrypath, entry_stat.st_size);
                    // break; not break to calc total file count
                }

                curr_file_index++;
            }

            total_image_file_count = curr_file_index;
            closedir(dir);
        }

        if (!finded) {
            current_bitmap_page_index = 0;
            image_page_draw(epd_paint, loop_cnt);
        }
    } else {
        // mount file failed use default
        current_bitmap_page_index = 0;
        image_page_draw(epd_paint, loop_cnt);
    }

    // draw battery icon if battery low
    int8_t battery_level = battery_get_level();
    if (battery_level >= 0 && battery_level < 20) {
        battery_view_t *battery_view = battery_view_create(battery_get_level(), 26, 16);
        battery_view_draw(battery_view, epd_paint, 4, 183);
        battery_view_deinit(battery_view);
    }
}

static void calc_total_image_file_count() {
    char entrypath[64];
    struct dirent *entry;
    /* Retrieve the base path of file storage to construct the full path */
    strcpy(entrypath, FILE_SERVER_BASE_PATH);
    strlcpy(entrypath + strlen(FILE_SERVER_BASE_PATH), "/", sizeof(entrypath) - strlen(FILE_SERVER_BASE_PATH));

    DIR *dir = opendir(entrypath);
    const size_t dirpath_len = strlen(entrypath);
    struct stat entry_stat;

    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", entrypath);
        total_image_file_count = 1;
    } else {
        ESP_LOGI(TAG, "list dir : %s", entrypath);
        uint16_t valid_file_count = 1;
        /* Iterate over all files / folders and fetch their names and sizes */
        while ((entry = readdir(dir)) != NULL) {
            if (!check_is_valid_image_file(entry, entrypath, dirpath_len, &entry_stat)) {
                continue;
            }
            valid_file_count++;
        }

        total_image_file_count = valid_file_count;
        ESP_LOGI(TAG, "calc total file count %d", total_image_file_count);
        closedir(dir);
    }
}

static esp_err_t delete_current_image() {
    if (current_bitmap_page_index == 0) {
        return ESP_FAIL;
    }

    struct stat entry_stat;
    if (stat(current_filepath, &entry_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat %s", current_filepath);
        return ESP_FAIL;
    }

    unlink(current_filepath);
    current_bitmap_page_index -= 1;
    if (total_image_file_count > 0) {
        total_image_file_count -= 1;
    }

    ESP_LOGI(TAG, "delete file %s", current_filepath);
    return ESP_OK;
}

bool image_page_key_click_handle(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_UP_SHORT_CLICK:
            current_bitmap_page_index -= 1;
            if (current_bitmap_page_index < 0) {
                if (total_image_file_count < 0) {
                    calc_total_image_file_count();
                }
                current_bitmap_page_index += total_image_file_count;
            }
            page_manager_request_update(false);
            return true;
        case KEY_DOWN_SHORT_CLICK:
            current_bitmap_page_index += 1;
            page_manager_request_update(false);
            return true;
        case KEY_CANCEL_SHORT_CLICK:
            // show delete menu
            page_manager_show_menu("confirm-alert", &confirm_menu_arg);
            page_manager_request_update(false);
            return true;
        case KEY_CANCEL_LONG_CLICK: {
            // show alert dialog
            ble_server_init();

            static alert_dialog_arg_t alert_dialog_arg = {
                    .title_label = (char *) text_ble_on,
                    .auto_close_ms = 5000
            };
            page_manager_show_menu("alert-dialog", &alert_dialog_arg);
            page_manager_request_update(false);
            return true;
        }
        default:
            return false;
    }

    return false;
}

void image_page_on_destroy(void *arg) {
    unmount_storage();
    file_system_mounted = false;
}

int image_page_on_enter_sleep(void *args) {
    return 5400;
}
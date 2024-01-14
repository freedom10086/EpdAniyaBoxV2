//
// Created by yang on 2024/1/10.
//

#include "esp_log.h"
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_vfs.h"

#include "setting.h"
#include "rx8025t.h"
#include "wifi/my_file_server_common.h"

#define TAG "BOX_SETTING"

#define MAX_BMP_FILE_SIZE 8192
#define CHECK_BMP_UPLOAD_TIMEOUT 200

static uint8_t ping = 0;
static SemaphoreHandle_t xSemaphore = NULL;
static TaskHandle_t tsk_hdl = NULL;

static uint8_t current_bmp_file_id = 0;
static uint16_t current_bmp_file_size = 0;
static uint16_t current_bmp_file_write_size = 0;
static FILE *current_fd = NULL;
static char bmp_filepath[ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN];

static esp_err_t open_file(uint8_t file_id, uint16_t file_size);

static esp_err_t write_file(uint16_t offset, uint8_t *data, uint16_t data_len);

static void check_upload_task_entry(void *arg);

/**
 *  0-6
 * [year month day week hour minute second]
 */
esp_err_t box_setting_load(uint8_t cmd, uint8_t *out, uint16_t *out_len) {
    esp_err_t err;
    uint16_t len = 0;

    box_setting_time_t t;
    err = rx8025t_get_time(&t.year, &t.month, &t.day, &t.week, &t.hour, &t.minute, &t.second);
    if (err != ESP_OK) {
        return err;
    }

    out[0] = t.year;
    out[1] = t.month;
    out[2] = t.day;
    out[3] = t.week;
    out[4] = t.hour;
    out[5] = t.minute;
    out[6] = t.second;
    len += 7;

    *out_len = len;
    return err;
}

esp_err_t box_setting_apply(uint8_t cmd, uint8_t *data, uint16_t data_len) {
    esp_err_t err;
    switch (cmd) {
        case BOX_SETTING_CMD_SET_TIME:
            if (data_len != 7) {
                ESP_LOGW(TAG, "BOX_SETTING_CMD_SET_TIME data len should be 7 but %d", data_len);
                return ESP_ERR_INVALID_ARG;
            }
            err = rx8025t_set_time(data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
            ESP_LOGI(TAG, "set time %d %d %d %d %d %d %d, res:%d", data[0], data[1], data[2], data[3], data[4], data[5],
                     data[6], err);
            if (err != ESP_OK) {
                return err;
            }
            return ESP_OK;
        case BOX_SETTING_CMD_UPLOAD_BMP_START: {
            uint8_t file_id = data[0];
            if (file_id == current_bmp_file_id && file_id > 0) {
                return ESP_OK;
            }

            if (xSemaphore != NULL || file_id == 0 || tsk_hdl != NULL) {
                return ESP_FAIL;
            }

            current_bmp_file_write_size = 0;
            current_bmp_file_size = data[1] + (data[2] << 8);

            // mount spiffs
            ESP_ERROR_CHECK(mount_storage(FILE_SERVER_BASE_PATH, true));
            err = open_file(file_id, current_bmp_file_size);
            if (err != ESP_OK) {
                return err;
            }

            BaseType_t creat_task_err = xTaskCreate(
                    check_upload_task_entry,
                    "check_upload_task",
                    2048,
                    NULL,
                    uxTaskPriorityGet(NULL),
                    &tsk_hdl);
            if (creat_task_err != pdTRUE) {
                fclose(current_fd);
                unlink(bmp_filepath);
                tsk_hdl = NULL;
                return ESP_FAIL;
            }

            current_bmp_file_id = file_id;
            ESP_LOGI(TAG, "upload bmp task started... fileId:%d file_size:%d", file_id, current_bmp_file_size);

            xSemaphore = xSemaphoreCreateBinary();
            xSemaphoreGive(xSemaphore);
            return write_file(0, data + 3, data_len - 3);
        }
        case BOX_SETTING_CMD_UPLOAD_BMP_DATA: {
            uint8_t file_id = data[0];
            if (file_id != current_bmp_file_id || file_id == 0) {
                return ESP_FAIL;
            }
            uint16_t offset = data[1] + (data[2] << 8);
            return write_file(offset, data + 3, data_len - 3);
        }
        default:
            ESP_LOGW(TAG, "un supported cmd:%d len:%d", cmd, data_len);
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t open_file(uint8_t file_id, uint16_t file_size) {
    uint16_t rndId = xTaskGetTickCount() % 60000;
    struct stat file_stat;
    do {
        sprintf(bmp_filepath, "%s/b%d.bmp", FILE_SERVER_BASE_PATH, rndId);
        rndId += 1;
        ESP_LOGI(TAG, "bmp file path %s for file id %d", bmp_filepath, file_id);
    } while (stat(bmp_filepath, &file_stat) == 0); // == 0  file exist

    /* File cannot be larger than a limit */
    if (file_size > MAX_BMP_FILE_SIZE) {
        ESP_LOGE(TAG, "bmp File too large : %d bytes must < %d bytes", file_size, MAX_BMP_FILE_SIZE);
        return ESP_FAIL;
    }

    current_fd = fopen(bmp_filepath, "w");
    if (current_fd == NULL) {
        ESP_LOGE(TAG, "Failed to create bmp file : %s", bmp_filepath);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t write_file(uint16_t offset, uint8_t *data, uint16_t data_len) {
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    fseek(current_fd, offset, SEEK_SET);
    uint16_t write_len = fwrite(data, 1, data_len, current_fd);
    if (write_len != data_len) {
        return ESP_FAIL;
    }
    current_bmp_file_write_size += write_len;
    ESP_LOGI(TAG, "write bmp file fileId:%d size:%d remain:%d", current_bmp_file_id, write_len,
             current_bmp_file_size - current_bmp_file_write_size);
    xSemaphoreGive(xSemaphore);
    xTaskGenericNotify(tsk_hdl, 0, data_len,
                       eIncrement, NULL);
    return ESP_OK;
}

static void check_upload_task_entry(void *arg) {
    while (current_bmp_file_write_size < current_bmp_file_size) {
        uint32_t ulNotificationCount = ulTaskGenericNotifyTake(0, pdTRUE, pdMS_TO_TICKS(CHECK_BMP_UPLOAD_TIMEOUT));
        uint16_t remain = current_bmp_file_size - current_bmp_file_write_size;
        ESP_LOGI(TAG, "get write notify %ld fileId:%d remain:%d", ulNotificationCount, current_bmp_file_id, remain);
        if (remain == 0) {
            break;
        }
        if (ulNotificationCount > 0) {
            // success get
        } else {
            // timeout
            break;
        }
    }

    ESP_LOGI(TAG, "check upload bmp task finish fileId:%d name:%s", current_bmp_file_id, bmp_filepath);

    fclose(current_fd);
    current_bmp_file_id = 0;

    vSemaphoreDelete(xSemaphore);
    xSemaphore = NULL;

    if (current_bmp_file_write_size != current_bmp_file_size) {
        // failed
        unlink(bmp_filepath);
        ESP_LOGW(TAG, "upload bmp file failed fileId:%d name:%s expectSize: %d actualSize:%d", current_bmp_file_id, bmp_filepath,
                 current_bmp_file_size, current_bmp_file_write_size);
    } else {
        ESP_LOGI(TAG, "upload bmp file success fileId:%d name:%s", current_bmp_file_id, bmp_filepath);
    }

    tsk_hdl = NULL;
    vTaskDelete(NULL);
}

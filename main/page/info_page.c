#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include "esp_mac.h"
#include "esp_sleep.h"

#include "common_utils.h"
#include "lcd/epdpaint.h"
#include "battery.h"

#include "info_page.h"
#include "lcd/display.h"
#include "lcd/epd_lcd_ssd1680.h"
#include "page_manager.h"
#include "max31328.h"


/*********************
 *      DEFINES
 *********************/
#define TAG "info-page"

static char info_page_draw_text_buf[64] = {0};

bool wifi_on = false;
esp_app_desc_t running_app_info;
const esp_partition_t *running_partition;
extern uint32_t boot_count;

bool info_page_key_click(key_event_id_t key_event_type) {
    if (key_event_type == KEY_FN_SHORT_CLICK || key_event_type == KEY_OK_SHORT_CLICK) {
        page_manager_close_page();
        page_manager_request_update(false);
        return true;
    }
    return false;
}

void info_page_on_create(void *arg) {
    running_partition = esp_ota_get_running_partition();
    esp_ota_get_partition_description(running_partition, &running_app_info);
}

void info_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);
    uint16_t y = 0;

    sprintf(info_page_draw_text_buf, "AniyaBox Beta");
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font20, 1);
    y += 22;

    // battery
    int battery_voltage = battery_get_voltage();
    int8_t battery_level = battery_get_level();

    epd_paint_draw_string_at(epd_paint, 0, y, "battery:", &Font20, 1);
    y += 20;

    sprintf(info_page_draw_text_buf, "%dmv %d%%", battery_voltage * 2, battery_level);
    epd_paint_draw_string_at(epd_paint, 16, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // heap
    sprintf(info_page_draw_text_buf, "free heap:%ld", esp_get_free_heap_size());
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // partition
    sprintf(info_page_draw_text_buf, "ota subtype:%d", running_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // version
    sprintf(info_page_draw_text_buf, "build:%s", running_app_info.date);
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // loop and boot info
    sprintf(info_page_draw_text_buf, "boot:%ld cause:%d", boot_count, esp_sleep_get_wakeup_cause());
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // time
    //sprintf(info_page_draw_text_buf, "build time:%s", running_app_info.time);
    //epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    //y += 18;

    // current date time
    uint8_t year, month, day, week, hour, minute, second;
    max31328_get_time(&year, &month, &day, &week, &hour, &minute, &second);
    sprintf(info_page_draw_text_buf, "20%d-%d-%d %d:%d:%d %d", year, month, day, hour, minute, second, week);
    epd_paint_draw_string_at(epd_paint, 0, y, info_page_draw_text_buf, &Font16, 1);
    y += 18;

    // version
    sprintf(info_page_draw_text_buf, "%s", running_app_info.version);
    epd_paint_draw_string_at(epd_paint, 0, LCD_V_RES - 1 - 18, info_page_draw_text_buf, &Font16, 1);
}

int info_page_on_enter_sleep(void *args) {
    return DEFAULT_SLEEP_TS;
}
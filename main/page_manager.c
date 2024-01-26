#include "stdio.h"
#include "esp_log.h"
#include "string.h"

#include "page_manager.h"
#include "common_utils.h"

#include "page/test_page.h"
#include "page/info_page.h"
#include "page/image_page.h"
#include "page/temperature_page.h"
#include "page/upgrade_page.h"
#include "page/menu_page.h"
#include "page/confirm_menu_page.h"
#include "page/alert_dialog_page.h"
#include "page/manual_page.h"
#include "page/image_manage_page.h"
#include "page/setting_list_page.h"
#include "page/ble_device_page.h"
#include "page/data_time_page.h"
#include "page/battery_page.h"
#include "page/music_page.h"
#include "page/tomato_clock.h"
#include "battery.h"
#include "rx8025t.h"

#define TAG "page-manager"
#define TOTAL_PAGE 13
#define TOTAL_MENU 3

static int8_t pre_page_index = -1;
static int8_t menu_index = -1;
extern esp_event_loop_handle_t event_loop_handle;
static QueueHandle_t event_queue;

RTC_DATA_ATTR static int8_t current_page_index = -1;

static void key_event_task_entry(void *arg);

static page_inst_t pages[] = {
        [TEMP_PAGE_INDEX] = {
                .page_name = "temperature",
                .on_draw_page = temperature_page_draw,
                .key_click_handler = temperature_page_key_click_handle,
                .on_create_page = temperature_page_on_create,
                .on_destroy_page = temperature_page_on_destroy
        },
        [IMAGE_PAGE_INDEX] = {
                .page_name = "image",
                .on_draw_page = image_page_draw,
                .key_click_handler = image_page_key_click_handle,
                .on_create_page = image_page_on_create,
                .on_destroy_page = image_page_on_destroy,
                .enter_sleep_handler = image_page_on_enter_sleep,
        },
        [DATE_TIME_PAGE_INDEX] = {
                .page_name = "date-time",
                .on_create_page = date_time_page_on_create,
                .on_draw_page = date_time_page_draw,
                .key_click_handler = date_time_page_key_click,
                .on_destroy_page = date_time_page_on_destroy,
                .enter_sleep_handler = date_time_page_on_enter_sleep,
        },
        [TOMATO_PAGE_INDEX] = {
                .page_name = "tomato",
                .on_create_page = tomato_page_on_create,
                .on_draw_page = tomato_page_draw,
                .key_click_handler = tomato_page_key_click,
                .on_destroy_page = tomato_page_on_destroy,
                .enter_sleep_handler = tomato_page_on_enter_sleep,
        },
        {
                .page_name = "info",
                .on_draw_page = info_page_draw,
                .on_create_page = info_page_on_create,
                .key_click_handler = info_page_key_click,
                .enter_sleep_handler = info_page_on_enter_sleep,
        },
        {
                .page_name = "test",
                .on_create_page = test_page_on_create,
                .on_draw_page = test_page_draw,
                .key_click_handler = test_page_key_click,
                .on_destroy_page = test_page_on_destroy,
                .enter_sleep_handler = test_page_on_enter_sleep,
        },
        {
                .page_name = "upgrade",
                .on_draw_page = upgrade_page_draw,
                .key_click_handler = upgrade_page_key_click_handle,
                .on_create_page = upgrade_page_on_create,
                .on_destroy_page = upgrade_page_on_destroy,
                .enter_sleep_handler = upgrade_page_on_enter_sleep,
        },
        {
                .page_name = "manual",
                .on_draw_page = manual_page_draw,
                .key_click_handler = manual_page_key_click,
                .on_create_page = manual_page_on_create,
                .on_destroy_page = manual_page_on_destroy,
        },
        {
                .page_name = "image-manage",
                .on_draw_page = image_manage_page_draw,
                .key_click_handler = image_manage_page_key_click,
                .on_create_page = image_manage_page_on_create,
                .on_destroy_page = image_manage_page_on_destroy,
                .enter_sleep_handler = image_manage_page_on_enter_sleep,
        },
        {
                .page_name = "setting-list",
                .on_draw_page = setting_list_page_draw,
                .key_click_handler = setting_list_page_key_click,
                .on_create_page = setting_list_page_on_create,
                .on_destroy_page = setting_list_page_on_destroy,
                .enter_sleep_handler = setting_list_page_on_enter_sleep,
                .after_draw_page = setting_list_page_after_draw,
        },
        {
                .page_name = "ble-device",
                .on_draw_page = ble_device_page_draw,
                .key_click_handler = ble_device_page_key_click,
                .on_create_page = ble_device_page_on_create,
                .on_destroy_page = ble_device_page_on_destroy,
                .enter_sleep_handler = ble_device_page_on_enter_sleep,
                .after_draw_page = ble_device_page_after_draw,
        },

        {
                .page_name = "battery",
                .on_draw_page = battery_page_draw,
                .on_create_page = battery_page_on_create,
                .key_click_handler = battery_page_key_click,
                .enter_sleep_handler = battery_page_on_enter_sleep,
                .on_destroy_page = battery_page_on_destroy,
        },
        {
                .page_name = "music",
                .on_draw_page = music_page_draw,
                .on_create_page = music_page_on_create,
                .key_click_handler = music_page_key_click,
                .enter_sleep_handler = music_page_on_enter_sleep,
                .on_destroy_page = music_page_on_destroy,
        },
};

static page_inst_t menus[] = {
        [0] = {
                .page_name = "menu",
                .on_draw_page = menu_page_draw,
                .key_click_handler = menu_page_key_click,
                .on_create_page = menu_page_on_create,
                .on_destroy_page = menu_page_on_destroy,
                .after_draw_page = menu_page_after_draw,
        },
        [1] = {
                .page_name = "confirm-alert",
                .on_draw_page = confirm_menu_page_draw,
                .key_click_handler = confirm_menu_page_key_click,
                .on_create_page = confirm_menu_page_on_create,
                .on_destroy_page = confirm_menu_page_on_destroy,
                .after_draw_page = confirm_menu_page_after_draw,
        },
        [2] = {
                .page_name = "alert-dialog",
                .on_draw_page = alert_dialog_page_draw,
                .key_click_handler = alert_dialog_page_key_click,
                .on_create_page = alert_dialog_page_on_create,
                .on_destroy_page = alert_dialog_page_on_destroy,
                .after_draw_page = alert_dialog_page_after_draw,
        }
};

bool page_manager_switch_page_by_index(int8_t dest_page_index, bool push_stack);

static void key_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    xQueueSend(event_queue, (void *) &event_id, pdMS_TO_TICKS(10));
}

void page_manager_init(int8_t page_index) {
    event_queue = xQueueCreate(10, sizeof(int32_t));
    TaskHandle_t tsk_hdl;
    /* Create key click detect task */
    BaseType_t err = xTaskCreate(
            key_event_task_entry,
            "page_manage_task",
            3072,
            NULL,
            uxTaskPriorityGet(NULL),
            &tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create page manager task failed");
    }

    esp_event_handler_register_with(event_loop_handle,
                                    BIKE_KEY_EVENT, ESP_EVENT_ANY_ID,
                                    key_event_handler, NULL);

    // reset to -1 when awake from deep sleep
    current_page_index = -1;
    page_manager_switch_page_by_index(page_index, false);
}

int8_t page_manager_get_current_index() {
    return current_page_index;
}

bool page_manager_switch_page_by_index(int8_t dest_page_index, bool push_stack) {
    if (current_page_index == dest_page_index) {
        ESP_LOGW(TAG, "dest page is current %d", dest_page_index);
        return false;
    }
    if (dest_page_index < 0) {
        ESP_LOGE(TAG, "dest page is invalid %d", dest_page_index);
        return false;
    }

    ESP_LOGI(TAG, "page switch from %s to %s ",
             current_page_index >= 0 ? pages[current_page_index].page_name : "empty",
             pages[dest_page_index].page_name);

    if (push_stack) {
        pages[dest_page_index].parent_page_index = current_page_index;
    }
//    else {
//        pages[dest_page_index].parent_page_index = -1;
//    }

    pre_page_index = current_page_index;

    // old page destroy
    if (pre_page_index >= 0 && pages[pre_page_index].on_destroy_page != NULL) {
        pages[pre_page_index].on_destroy_page(NULL);
        ESP_LOGI(TAG, "page %s on destroy", pages[pre_page_index].page_name);
    }

    // new page on create
    if (pages[dest_page_index].on_create_page != NULL) {
        pages[dest_page_index].on_create_page(&current_page_index);
        ESP_LOGI(TAG, "page %s on create", pages[dest_page_index].page_name);
    }

    current_page_index = dest_page_index;
    return true;
}

bool page_manager_switch_page(char *page_name, bool push_stack) {
    int8_t idx = -1;
    for (int8_t i = 0; i < TOTAL_PAGE; ++i) {
        if (strcmp(pages[i].page_name, page_name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        ESP_LOGE(TAG, "can not find page %s", page_name);
        return page_manager_switch_page_by_index(0, push_stack);
    }
    return page_manager_switch_page_by_index(idx, push_stack);
}

bool page_manager_close_page() {
    page_inst_t curr_page = page_manager_get_current_page();
    if (curr_page.parent_page_index >= 0) {
        return page_manager_switch_page_by_index(curr_page.parent_page_index, false);
    } else {
        if (current_page_index < HOME_PAGE_COUNT) {
            ESP_LOGW(TAG, "current page %s is home page cant close", curr_page.page_name);
            return false;
        }

        if (pre_page_index == -1) {
            ESP_LOGW(TAG, "no pre page return to temp page");
            return page_manager_switch_page_by_index(TEMP_PAGE_INDEX, false);
        } else {
            ESP_LOGW(TAG, "no parent page switch to pre page");
            return page_manager_switch_page_by_index(pre_page_index, false);
        }
    }
}

page_inst_t page_manager_get_current_page() {
    page_inst_t current_page = pages[current_page_index];
    return current_page;
}

bool page_manager_has_menu() {
    return menu_index != -1;
}

page_inst_t page_manager_get_current_menu() {
    page_inst_t current_menu = menus[menu_index];
    return current_menu;
}

void page_manager_show_menu(char *name, void *args) {
    int8_t idx = -1;
    for (int8_t i = 0; i < TOTAL_MENU; ++i) {
        if (strcmp(menus[i].page_name, name) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        ESP_LOGE(TAG, "can not find menu %s", name);
        return;
    }

    if (menu_index == idx) {
        ESP_LOGW(TAG, "menu %s already exist", name);
        return;
    }

    if (menu_index != -1) {
        // close current menu
        page_manager_close_menu();
    }

    // new page on create
    if (menus[idx].on_create_page != NULL) {
        menus[idx].on_create_page(args);
        ESP_LOGI(TAG, "menu %s on create", menus[idx].page_name);
    }
    menu_index = idx;
}

void page_manager_close_menu() {
    if (menu_index != -1) {
        if (menus[menu_index].on_destroy_page != NULL) {
            menus[menu_index].on_destroy_page(NULL);
            ESP_LOGI(TAG, "menu %s on destroy", menus[menu_index].page_name);
        }
        menu_index = -1;
    }
}

// return sleep ts, -1 stop sleep, 0 never wake up by timer
int page_manager_enter_sleep(uint32_t loop_cnt) {
    if (page_manager_has_menu()) {
        page_manager_close_menu();
        page_manager_request_update(false);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    page_inst_t current_page = page_manager_get_current_page();
    if (current_page.enter_sleep_handler != NULL) {
        int sleepTs = current_page.enter_sleep_handler((void *) loop_cnt);
        if (sleepTs != NO_SLEEP_TS) {
            return sleepTs;
        }
    }

    int8_t battery_level = battery_get_level();
    if (battery_is_charge()) {
        ESP_LOGI(TAG, "battery is charge use default sleep time");
        return DEFAULT_SLEEP_TS;
    }
    if (battery_level < 5 && battery_level >= 0) {
        // battery low never wake up
        return NEVER_WAKE_SLEEP_TS;
    }

    bool in_night = false;
    rx8025_time_t t;
    esp_err_t load_time_err = rx8025t_get_time(&t.year, &t.month, &t.day, &t.week, &t.hour, &t.minute, &t.second);
    if (load_time_err == ESP_OK) {
        in_night = (t.year >= 24 && t.year <= 35) && (t.hour >= 23 || t.hour <= 9);
    }
    if (in_night) {
        return NIGHT_SLEEP_TS;
    }
    if (battery_level < 30 && battery_level >= 0) {
        return DEFAULT_SLEEP_TS * 5;
    }
    // battery is invalid use default sleep time
    return DEFAULT_SLEEP_TS;
}

void page_manager_request_update(uint32_t full_refresh) {
    common_post_event_data(BIKE_REQUEST_UPDATE_DISPLAY_EVENT, 0, (void *) full_refresh, sizeof(full_refresh));
}

static void key_event_task_entry(void *arg) {
    int32_t event_id;
    while (1) {
        if (xQueueReceive(event_queue, &event_id, portMAX_DELAY)) {
            //ESP_LOGI(TAG, "rev key click event %ld", event_id);
            // if menu exist
            if (page_manager_has_menu()) {
                page_inst_t current_menu = page_manager_get_current_menu();
                if (current_menu.key_click_handler) {
                    if (current_menu.key_click_handler(event_id)) {
                        continue;
                    }
                }
            }

            // if not handle passed to view
            page_inst_t current_page = page_manager_get_current_page();
            if (current_page.key_click_handler) {
                if (current_page.key_click_handler(event_id)) {
                    continue;
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
                        continue;
                    } else {
                        if (page_manager_close_page()) {
                            page_manager_request_update(false);
                        }
                        continue;
                    }
                    break;
                case KEY_OK_SHORT_CLICK:
                    if (page_manager_get_current_index() < HOME_PAGE_COUNT) {
                        if (page_manager_has_menu()) {
                            page_manager_close_menu();
                            page_manager_request_update(false);
                            continue;
                        } else {
                            page_manager_show_menu("menu", NULL);
                            page_manager_request_update(false);
                            continue;
                        }
                    }
                    break;
                case KEY_OK_LONG_CLICK:
                    if (page_manager_has_menu()) {
                        page_manager_close_menu();
                        page_manager_request_update(false);
                        continue;
                    } else {
                        page_manager_show_menu("menu", NULL);
                        page_manager_request_update(false);
                        continue;
                    }
                    break;
                case KEY_UP_LONG_CLICK:
                case KEY_DOWN_LONG_CLICK:
                    if (page_manager_get_current_index() < HOME_PAGE_COUNT) {
                        int8_t dest_index =
                                (page_manager_get_current_index() + ((event_id == KEY_UP_LONG_CLICK) ? -1 : 1) +
                                 HOME_PAGE_COUNT) % HOME_PAGE_COUNT;
                        if (page_manager_switch_page_by_index(dest_index, false)) {
                            page_manager_request_update(false);
                        }
                        continue;
                    }
                    break;
                default:
                    break;
            }

            // if page not handle key click event here handle
            ESP_LOGI(TAG, "no page handler key click event %ld", event_id);
        }
    }
}
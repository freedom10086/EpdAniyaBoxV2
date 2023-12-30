#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <esp_log.h>

#include "common_utils.h"
#include "list_view.h"

#define DIVIDER 1
#define PADDING_TOP 1
#define PADDING_BOTTOM 1
#define PADDING_START 4
#define PADDING_END 4

#define TAG "list-view"

list_view_t *list_vew_create(int x, int y, int width, int height, sFONT *font) {
    ESP_LOGI(TAG, "init list view");
    list_view_t *list_view = malloc(sizeof(list_view_t));
    if (!list_view) {
        ESP_LOGE(TAG, "no memory for init list_view");
        return NULL;
    }

    list_view->element_count = 0;
    list_view->current_index = -1;
    list_view->head = NULL;

    list_view->x = x;
    list_view->y = y;
    list_view->width = width;
    list_view->height = height;
    list_view->font = font;

    list_view->current_item_offset = 0;

    ESP_LOGI(TAG, "list view created");
    return list_view;
}

void list_view_add_element(list_view_t *list_view, char *text) {
    ESP_LOGI(TAG, "list view add item");
    struct list_view_element_t *head = list_view->head;
    while (head != NULL && head->next != NULL) {
        head = head->next;
    }

    struct list_view_element_t *ele = malloc(sizeof(struct list_view_element_t));
    if (!ele) {
        ESP_LOGE(TAG, "no memory for new list_view element");
    }

    ele->text = calloc(strlen(text), sizeof(char));
    strcpy(ele->text, text);

    //ele->text = text;
    ele->next = NULL;

    if (head == NULL) {
        // first element
        list_view->head = ele;
    } else { // head -> next is null
        head->next = ele;
    }

    list_view->element_count += 1;
    if (list_view->current_index == -1) {
        list_view->current_index = 0;
    }
}

struct list_view_element_t *list_view_get_element(list_view_t *list_view, uint8_t index) {
    assert(index < list_view->element_count);
    struct list_view_element_t *pre = NULL;
    struct list_view_element_t *item = list_view->head;
    for (; index > 0; index--) {
        item = item->next;
    }
    return item;
}

void list_view_update_item(list_view_t *list_view, uint8_t index, char *new_text) {
    struct list_view_element_t *ele = list_view_get_element(list_view, index);
    assert(ele != NULL);
    if (strlen(ele->text) < strlen(new_text)) {
        free(ele->text);
        ele->text = calloc(strlen(new_text), sizeof(char));
    }
    strcpy(ele->text, new_text);
}

void list_view_remove_element(list_view_t *list_view, uint8_t index) {
    assert(index < list_view->element_count);
    struct list_view_element_t *pre = NULL;
    struct list_view_element_t *item = list_view->head;
    for (; index > 0; index--) {
        pre = item;
        item = item->next;
    }

    if (pre == NULL) {
        list_view->head = item->next;
    } else {
        pre->next = item->next;
    }

    list_view->element_count -= 1;
    if (list_view->current_index >= list_view->element_count) {
        list_view->current_index = list_view->element_count - 1;
    }

    item->next = NULL;
    if (item->text) {
        free(item->text);
    }
    free(item);
}

void list_view_remove_first_element(list_view_t *list_view) {
    list_view_remove_element(list_view, 0);
}

void list_view_remove_last_element(list_view_t *list_view) {
    list_view_remove_element(list_view, list_view->element_count - 1);
}

void list_vew_draw(list_view_t *list_view, epd_paint_t *epd_paint, uint32_t loop_cnt) {

    epd_paint_clear_range(epd_paint, list_view->x, list_view->y,
                          list_view->width, list_view->height, 0);

    if (list_view->element_count == 0) {
        return;
    }

    int y = list_view->y;
    int x = list_view->x;

    // calc select item start y if not in display range try to scroll up
    uint8_t item_height = (PADDING_TOP + list_view->font->Height + PADDING_BOTTOM) + DIVIDER;
    int selected_item_offset_start_y = item_height * (list_view->current_index - list_view->current_item_offset);
    int selected_item_offset_end_y = selected_item_offset_start_y + item_height;

    while(selected_item_offset_start_y < 0) {
        selected_item_offset_start_y += item_height;
        selected_item_offset_end_y += item_height;
        list_view->current_item_offset -= 1;
    }

    while (selected_item_offset_end_y > list_view->height) {
        selected_item_offset_end_y -= item_height;
        selected_item_offset_start_y -= item_height;
        list_view->current_item_offset += 1;
    }

    struct list_view_element_t *head = list_view->head;

    int item_start_y;
    int index = 0;
    while (head != NULL) {
        if (index < list_view->current_item_offset) {
            head = head->next;
            index++;
            continue;
        }

        item_start_y = y;

        y += PADDING_TOP;
        epd_paint_draw_string_at(epd_paint, x + PADDING_START, y, head->text, list_view->font, 1);
        y += list_view->font->Height;
        y += PADDING_BOTTOM;

        head = head->next;
        if (head != NULL && DIVIDER) {
            // not last draw divider
            for (int i = 0; i < DIVIDER; ++i) {
                epd_paint_draw_horizontal_line(epd_paint, x, y, list_view->width, 1);
                y += 1;
            }
        }

        // selected item
        if (index == list_view->current_index) {
            epd_paint_reverse_range(epd_paint,
                                    x, item_start_y + PADDING_TOP,
                                    list_view->width,
                                    list_view->font->Height);
        }
        index++;
    }
}

int list_view_get_select_index(list_view_t *list_view) {
    return list_view->current_index;
}

int list_view_set_select_index(list_view_t *list_view, int index) {
    assert(index >= 0 && index < list_view->element_count);
    int pre_index = list_view->current_index;
    list_view->current_index = index;
    return pre_index;
}

bool list_view_select_next(list_view_t *list_view) {
    uint8_t next_index = (list_view->current_index + 1) % list_view->element_count;
    list_view_set_select_index(list_view, next_index);
    return true;
}

bool list_view_select_pre(list_view_t *list_view) {
    uint8_t pre_index = (list_view->current_index - 1 + list_view->element_count) % list_view->element_count;
    list_view_set_select_index(list_view, pre_index);
    return true;
}

void list_view_deinit(list_view_t *list_view) {
    struct list_view_element_t *head = list_view->head;
    struct list_view_element_t *ele = NULL;

    while (head != NULL) {
        ele = head;
        head = head->next;

        if (ele->text) {
            free(ele->text);
        }
        free(ele);
    }

    free(list_view);
}
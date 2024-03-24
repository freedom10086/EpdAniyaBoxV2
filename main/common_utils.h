//
// Created by yang on 2023/7/28.
//

#ifndef EPD_ANIYA_BOX_COMMON_UTILS_H
#define EPD_ANIYA_BOX_COMMON_UTILS_H

#include <string.h>
#include "host/ble_hs.h"
#include "esp_types.h"
#include "esp_event.h"
#include "esp_event_base.h"

#define max(a, b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

#define min(a, b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})

#define mabs(a)             \
({                           \
    __typeof__ (a) _a = (a); \
    _a < 0 ? -a : a;       \
})

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)

esp_err_t common_post_event(esp_event_base_t event_base, int32_t event_id);

esp_err_t common_post_event_data(esp_event_base_t event_base, int32_t event_id, const void *event_data, size_t event_data_size);

esp_err_t common_init_nvs();

/** Misc. */
void print_bytes(const uint8_t *bytes, int len);

void print_mbuf(const struct os_mbuf *om);

void print_mbuf_data(const struct os_mbuf *om);

char *addr_str(const void *addr);

void print_uuid(const ble_uuid_t *uuid);

void print_addr(const void *addr);

void print_conn_desc(const struct ble_gap_conn_desc *desc);

void print_adv_fields(const struct ble_hs_adv_fields *fields);

void ext_print_adv_report(const void *param);

#endif //EPD_ANIYA_BOX_COMMON_UTILS_H

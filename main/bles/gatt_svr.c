#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "gatts_sens.h"
#include "esp_log.h"
#include "esp_bt.h"

#include "ble_server.h"
#include "ble/my_ble_uuid.h"
#include "battery.h"
#include "sht40.h"
#include "setting.h"

#define TAG "BLE_SERVER_SVC"

#define READ_THROUGHPUT_PAYLOAD           32
#define WRITE_THROUGHPUT_PAYLOAD           PREFERRED_MTU_VALUE
#define NOTIFY_THROUGHPUT_PAYLOAD          32
#define MIN_REQUIRED_MBUF         2 /* Assuming payload of 500Bytes and each mbuf can take 292Bytes.  */

#define BLE_UUID_STR_LEN (37)
static char ble_uuid_buf[BLE_UUID_STR_LEN];

static uint8_t gatt_read_buff[READ_THROUGHPUT_PAYLOAD];
static uint8_t gatt_write_buff[WRITE_THROUGHPUT_PAYLOAD];

static SemaphoreHandle_t notify_sem;
static bool notify_state = false;
static TaskHandle_t notify_task_handle;

static uint16_t battery_notify_handle;
static uint16_t conn_handle;

static int gatt_svr_access_battery_level(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_access_environment_info(uint16_t conn_handle, uint16_t attr_handle,
                                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_access_uart(uint16_t conn_handle, uint16_t attr_handle,
                                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatts_test_svcs[] = {
        {
                .type = BLE_GATT_SVC_TYPE_PRIMARY,
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_BATTERY),
                .characteristics = (struct ble_gatt_chr_def[])
                        {
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_BATTERY_LEVEL),
                                        .access_cb = gatt_svr_access_battery_level,
                                        .val_handle = &battery_notify_handle,
                                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                                },
                                {
                                        0, /* No more characteristics in this service. */
                                }
                        },
        },
        {
                .type = BLE_GATT_SVC_TYPE_PRIMARY,
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_DEVICE_INFORMATION_UUID),
                .characteristics = (struct ble_gatt_chr_def[])
                        {
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_Manufacturer_Name),
                                        .access_cb = gatt_svr_chr_access_device_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_Model_Number),
                                        .access_cb = gatt_svr_chr_access_device_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_Serial_Number),
                                        .access_cb = gatt_svr_chr_access_device_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_Hardware_Revision),
                                        .access_cb = gatt_svr_chr_access_device_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_Firmware_Revision),
                                        .access_cb = gatt_svr_chr_access_device_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_Software_Revision),
                                        .access_cb = gatt_svr_chr_access_device_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        0, /* No more characteristics in this service. */
                                }
                        },
        },
        {
                .type = BLE_GATT_SVC_TYPE_PRIMARY,
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_ENVIRONMENTAL_SENSING_UUID),
                .characteristics = (struct ble_gatt_chr_def[])
                        {
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_TEMPERATURE),
                                        .access_cb = gatt_svr_chr_access_environment_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHAR_HUMIDITY),
                                        .access_cb = gatt_svr_chr_access_environment_info,
                                        .flags = BLE_GATT_CHR_F_READ
                                },
                                {
                                        0, /* No more characteristics in this service. */
                                }
                        }
        },
        {
                .type = BLE_GATT_SVC_TYPE_PRIMARY,
                .uuid = &BLE_SVC_UART_UUID.u,
                .characteristics = (struct ble_gatt_chr_def[])
                        {
                                {
                                        .uuid = &BLE_UUID_CHAR_UART_TX.u,
                                        .access_cb = gatt_svr_chr_access_uart,
                                        .flags = BLE_GATT_CHR_F_WRITE
                                },
                                {
                                        .uuid = &BLE_UUID_CHAR_UART_RX.u,
                                        .access_cb = gatt_svr_chr_access_uart,
                                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY
                                },
                                {
                                        0, /* No more characteristics in this service. */
                                }
                        }
        },
        {
                0, /* No more services. */
        },
};

static int gatt_svr_chr_write(uint16_t conn_handle, uint16_t attr_handle,
                              struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                              void *dst, uint16_t *len) {
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

void gatt_svr_handle_subscribe(struct ble_gap_event *event, void *arg) {
    if (event->subscribe.attr_handle == battery_notify_handle) {
        conn_handle = event->subscribe.conn_handle;
        notify_state = event->subscribe.cur_notify;
        if (arg != NULL) {
            ESP_LOGI(TAG, "notify arg = %d", *(int *) arg);
        }
        xSemaphoreGive(notify_sem);
    } else if (event->subscribe.attr_handle != battery_notify_handle) {
        notify_state = event->subscribe.cur_notify;
    }
}

void gatt_svr_notify_tx_event(struct ble_gap_event *event, void *arg) {
    if (event->notify_tx.attr_handle == battery_notify_handle) {
        if ((event->notify_tx.status == 0) || (event->notify_tx.status == BLE_HS_EDONE)) {
            /* Send new notification i.e. give Semaphore. By definition,
             * sending new notifications should not be based on successful
             * notifications sent, but let us adopt this method to avoid too
             * many `BLE_HS_ENOMEM` errors because of continuous transfer of
             * notifications.XXX */
            xSemaphoreGive(notify_sem);
        } else {
            ESP_LOGE(TAG, "BLE_GAP_EVENT_NOTIFY_TX notify tx status = %d", event->notify_tx.status);
        }
    }
}

static int gatt_svr_access_battery_level(uint16_t conn_handle, uint16_t attr_handle,
                                         struct ble_gatt_access_ctxt *ctxt, void *arg) {

    common_post_event(BIKE_BLE_SERVER_EVENT, BLE_SERVER_EVENT_READ_WRITE);

    uint16_t uuid16;
    int rc;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR
        || ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    } else {
        uuid16 = ble_uuid_u16(ctxt->dsc->uuid);
    }

    switch (uuid16) {
        case BLE_UUID_CHAR_BATTERY_LEVEL:
            if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
                uint8_t data[] = {battery_get_level()};
                rc = os_mbuf_append(ctxt->om, data, sizeof(uint8_t));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
                ESP_LOGI(TAG, "BLE_GATT_ACCESS_OP_READ_DSC");
                return 0;
            } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
                ESP_LOGI(TAG, "BLE_GATT_ACCESS_OP_WRITE_DSC");
                return 0;
            }
            return BLE_ATT_ERR_UNLIKELY;
        default:
            ESP_LOGE(TAG, "unknown op: %d for uuid: %x", ctxt->op, uuid16);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

static int gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt, void *arg) {

    common_post_event(BIKE_BLE_SERVER_EVENT, BLE_SERVER_EVENT_READ_WRITE);

    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int rc;
    switch (uuid16) {
        case BLE_UUID_CHAR_Manufacturer_Name:
            rc = os_mbuf_append(ctxt->om, "Aniya", sizeof("Aniya"));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        case BLE_UUID_CHAR_Model_Number:
            rc = os_mbuf_append(ctxt->om, "Aniya-Magic-Box", sizeof("Aniya-Magic-Box"));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        case BLE_UUID_CHAR_Serial_Number:
            rc = os_mbuf_append(ctxt->om, "Aniya-Magic-Box 2.0", sizeof("Aniya-Magic-Box 2.0"));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        case BLE_UUID_CHAR_Hardware_Revision:
            rc = os_mbuf_append(ctxt->om, "2.0", sizeof("2.0"));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        case BLE_UUID_CHAR_Firmware_Revision:
            rc = os_mbuf_append(ctxt->om, "2.0", sizeof("2.0"));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        case BLE_UUID_CHAR_Software_Revision:
            rc = os_mbuf_append(ctxt->om, "2.0", sizeof("2.0"));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        default:
            ESP_LOGE(TAG, "unknown op: %d for uuid: %x", ctxt->op, uuid16);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

static int gatt_svr_chr_access_environment_info(uint16_t conn_handle, uint16_t attr_handle,
                                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    common_post_event(BIKE_BLE_SERVER_EVENT, BLE_SERVER_EVENT_READ_WRITE);

    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int rc;
    float temp, hum;

    switch (uuid16) {
        case BLE_UUID_CHAR_TEMPERATURE:
            if (sht40_get_temp_hum(&temp, &hum) == ESP_OK) {
                ESP_LOGI(TAG, "BLE_UUID_CHAR_TEMPERATURE %.2f", temp);
                int16_t int16_temp = (int16_t)(temp * 100);
                uint8_t data[] = {int16_temp >> 8 & 0xff, int16_temp & 0xff};
                rc = os_mbuf_append(ctxt->om, data, sizeof(data));
            } else {
                rc = BLE_ATT_ERR_UNLIKELY;
            }
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        case BLE_UUID_CHAR_HUMIDITY:
            if (sht40_get_temp_hum(&temp, &hum) == ESP_OK) {
                ESP_LOGI(TAG, "BLE_UUID_CHAR_HUMIDITY %.2f", hum);
                uint16_t uint16_hum = (uint16_t)(hum * 100);
                uint8_t data[] = {uint16_hum >> 8 & 0xff, uint16_hum & 0xff};
                rc = os_mbuf_append(ctxt->om, data, sizeof(data));
            } else {
                rc = BLE_ATT_ERR_UNLIKELY;
            }
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        default:
            ESP_LOGE(TAG, "unknown op: %d for uuid: %x", ctxt->op, uuid16);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

static int gatt_svr_chr_access_uart(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg) {

    common_post_event(BIKE_BLE_SERVER_EVENT, BLE_SERVER_EVENT_READ_WRITE);

    int rc;
    const ble_uuid_t *uuid;
    uint16_t read_len;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR
        || ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uuid = ctxt->chr->uuid;
    } else {
        uuid = ctxt->dsc->uuid;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR
        && ble_uuid_cmp(uuid, &BLE_UUID_CHAR_UART_TX.u) == 0) {

        uint16_t write_len;
        rc = gatt_svr_chr_write(conn_handle, attr_handle,
                                ctxt->om, 0,
                                sizeof gatt_write_buff,
                                gatt_write_buff, &write_len);
        ESP_LOGI(TAG, "write uart tx len %d data[0]: %d, rc:%d", write_len, gatt_write_buff[0], rc);
        if (write_len == 0 || rc != ESP_OK) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        return box_setting_apply(gatt_write_buff[0], gatt_write_buff + 1, write_len - 1);
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR
        && ble_uuid_cmp(uuid, &BLE_UUID_CHAR_UART_RX.u) == 0) {
        rc = box_setting_load(BOX_SETTING_CMD_LOAD_CONFIG, gatt_read_buff, &read_len);
        if (rc != ESP_OK) {
            return rc;
        }
        rc = os_mbuf_append(ctxt->om, gatt_read_buff, read_len);
        ESP_LOGI(TAG, "read uart rx len %d data[0]: %d", read_len, gatt_read_buff[0]);
        return rc;
    } else {
        ble_uuid_to_str(uuid, ble_uuid_buf);
        ESP_LOGW(TAG, "unknown op: %d for uuid: %s", ctxt->op, ble_uuid_buf);
        rc = BLE_ATT_ERR_UNLIKELY;
    }

    return rc;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGI(TAG, "registered service %s with handle=%d",
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                     ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGI(TAG, "registering characteristic %s with "
                          "def_handle=%d val_handle=%d\n",
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                     ctxt->chr.def_handle,
                     ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGI(TAG, "registering descriptor %s with handle=%d",
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                     ctxt->dsc.handle);
            break;

        default:
            assert(0);
            break;
    }
}

/* This function sends notifications to the client */
static void notify_task(void *arg) {
    static uint8_t payload[NOTIFY_THROUGHPUT_PAYLOAD] = {0};/* Data payload */
    static uint8_t notify_count = 0;

    int rc;
    struct os_mbuf *om;

    payload[1] = rand();

    while (1) {
        ESP_LOGI(TAG, "notify_state = %d", notify_state);

        while (!notify_state) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG, "wait for notify semaphore");
        /* We are anyway using counting semaphore for sending
         * notifications. So hopefully not much waiting period will be
         * introduced before sending a new notification. Revisit this
         * counter if need to do away with semaphore waiting. XXX */
        xSemaphoreTake(notify_sem, portMAX_DELAY);

        /* Check if the MBUFs are available */
        if (os_msys_num_free() >= MIN_REQUIRED_MBUF) {
            do {
                payload[0] = battery_get_level();
                om = ble_hs_mbuf_from_flat(payload, 1);
                if (om == NULL) {
                    /* Memory not available for mbuf */
                    ESP_LOGE(TAG, "No MBUFs available from pool, retry..");
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
            } while (om == NULL);

            rc = ble_gatts_notify_custom(conn_handle, battery_notify_handle, om);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error while sending notification; rc = %d", rc);
                notify_count -= 1;
                xSemaphoreGive(notify_sem);
                /* Most probably error is because we ran out of mbufs (rc = 6),
                 * increase the mbuf count/size from menuconfig. Though
                 * inserting delay is not good solution let us keep it
                 * simple for time being so that the mbufs get freed up
                 * (?), of course assumption is we ran out of mbufs */
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        } else {
            ESP_LOGE(TAG, "Not enough OS_MBUFs available; reduce notify count ");
            xSemaphoreGive(notify_sem);
            notify_count -= 1;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        notify_count += 1;
        ESP_LOGI(TAG, "Notify  compete = %d", notify_count);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

int gatt_svr_init(void) {

    /* Create a counting semaphore for Notification. Can be used to track
     * successful notification txmission. Optimistically take some big number
     * for counting Semaphore */
    notify_sem = xSemaphoreCreateCounting(32, 0);

    sht40_init();

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N3);

    /* Initialize Notify Task */
    xTaskCreate(notify_task, "ble_notify_task", 4096, NULL, 10, &notify_task_handle);

    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatts_test_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatts_test_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int gatt_svr_deinit(void) {
    vTaskDelete(notify_task_handle);
    vSemaphoreDelete(notify_sem);
    return 0;
}

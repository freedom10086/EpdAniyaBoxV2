#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "common_utils.h"
#include "battery.h"

#define TAG "battery"
#define STORAGE_NAMESPACE "battery"

//ADC1 Channels io6
#define ADC1_CHAN     ADC_CHANNEL_6

ESP_EVENT_DEFINE_BASE(BIKE_BATTERY_EVENT);

static int _adc_raw;
static int _pre_pre_voltage = -1;
static int _pre_voltage = -1;
static int _voltage;

static TaskHandle_t battery_tsk_hdl;
// 电压曲线
uint32_t *battery_curve_data;
// 电压曲线大小(长度要除以 sizeof(uint32_t))
size_t battery_curve_size = 0;
bool start_battery_curve = false;

static uint32_t default_battery_curve_data[] = {
        2080, // 100%
        2000,// 80%
        1932,// 60%
        1888,// 40%
        1852,// 20%
        1603,// 0%
};

esp_err_t clear_battery_curve();

void init_power_gpio() {
    uint64_t bit_mask = 1ull << BATTERY_ADC_PWR_GPIO_NUM;
    gpio_config_t io_config = {
            .pin_bit_mask = bit_mask,
            .mode = GPIO_MODE_OUTPUT_OD, // open_drain mode
            .intr_type = GPIO_INTR_DISABLE,
            .pull_up_en = 0,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));
}

// onoff = 1 power on
void adc_power_on_off(uint8_t onoff) {
    if (onoff) {
        ESP_LOGI(TAG, "adc power on");
        gpio_set_level(BATTERY_ADC_PWR_GPIO_NUM, 0);
    } else {
        ESP_LOGI(TAG, "adc power off");
        gpio_set_level(BATTERY_ADC_PWR_GPIO_NUM, 1);
    }
}

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);

static void adc_calibration_deinit(adc_cali_handle_t handle);

bool battery_is_curving() {
    return start_battery_curve;
}

bool battery_start_curving() {
    if (start_battery_curve) {
        return true;
    }
    clear_battery_curve();
    battery_curve_size = 0;
    ESP_LOGI(TAG, "start battery curve...");
    start_battery_curve = true;
    return true;
}

bool battery_is_charge() {
    // 不准 大概吧 以后再优化
    return (_pre_voltage > 0 && _voltage - _pre_voltage >= 2)
           || (_pre_pre_voltage > 0 && _voltage >= _pre_voltage && _voltage - _pre_pre_voltage >= 2);
}

uint32_t battery_get_curving_data_count() {
    return battery_curve_size / sizeof(uint32_t);
}

esp_err_t clear_battery_curve() {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(my_handle, "curve", NULL, 0);
    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);

    ESP_LOGI(TAG, "clear battery curve data");
    return ESP_OK;
}

esp_err_t add_battery_curve(int v) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "curve", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    // Read previously saved blob if available
    uint32_t *votages = malloc(required_size + sizeof(uint32_t));
    if (required_size > 0) {
        err = nvs_get_blob(my_handle, "curve", votages, &required_size);
        if (err != ESP_OK) {
            free(votages);
            return err;
        }
    }

    // Write value including previously saved blob if available
    required_size += sizeof(uint32_t);
    votages[required_size / sizeof(uint32_t) - 1] = v;
    err = nvs_set_blob(my_handle, "curve", votages, required_size);
    battery_curve_size = required_size;

    free(votages);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t load_battery_curve(void) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // obtain required memory space to store blob being read from NVS
    err = nvs_get_blob(my_handle, "curve", NULL, &battery_curve_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (battery_curve_size == 0) {
        ESP_LOGI(TAG, "Nothing battery curve saved yet!\n");
    } else {
        battery_curve_data = malloc(battery_curve_size);
        err = nvs_get_blob(my_handle, "curve", battery_curve_data, &battery_curve_size);
        if (err != ESP_OK) {
            free(battery_curve_data);
            battery_curve_size = 0;
            ESP_LOGW(TAG, "Load battery curve data failed!\n");
            return err;
        }
        for (int i = 0; i < battery_curve_size / sizeof(uint32_t); i++) {
            // printf("%d: %ld\n", i + 1, battery_curve_data[i]);
            // pre handle battery curve data after master small or equals to before
            if (i > 0 && battery_curve_data[i] > battery_curve_data[i - 1]) {
                battery_curve_data[i] = battery_curve_data[i - 1];
            }
        }
    }

    // Close
    nvs_close(my_handle);
    ESP_LOGI(TAG, "Loaded battery curve data success, size: %d", battery_curve_size / sizeof(uint32_t));
    return ESP_OK;
}

// from 0 - 100, -1 is invalid
int battery_voltage_to_level(uint32_t input_voltage) {
    uint32_t curve_count = battery_curve_size / sizeof(uint32_t);
    if (curve_count <= 2) {
        return -1;
    }

    // skip first
    if (input_voltage >= battery_curve_data[1]) {
        ESP_LOGI(TAG, "battery level %02f", 100.0f);
        return 100;
    } else if (input_voltage <= battery_curve_data[curve_count - 2]) {
        ESP_LOGI(TAG, "battery level %02f", 0.0f);
        return 0;
    }

    // skip last
    float gap_size = 100.0f / ((float) curve_count - 1 - 2.0f);

    uint32_t finded_idx = curve_count - 2;
    uint32_t pre_aft_gap_len = 1;

    uint32_t pre = battery_curve_data[curve_count - 2];
    uint32_t aft = battery_curve_data[curve_count - 1];

    for (int i = 2; i < curve_count - 1; i++) {
        if (input_voltage >= battery_curve_data[i]) {
            finded_idx = i;
            pre = battery_curve_data[i - 1];

            while (i + 1 < curve_count - 1 && battery_curve_data[i + 1] >= battery_curve_data[i]) {
                pre_aft_gap_len++;
                i++;
            }

            aft = battery_curve_data[i];
            break;
        }
    }

    float level = 100.0f - (float) finded_idx * gap_size -
                  (pre <= aft ? 0 : ((float) pre - (float) input_voltage) / ((float) pre - (float) aft) * gap_size *
                                    (float) pre_aft_gap_len);
    ESP_LOGI(TAG, "battery level %02f", level);
    return (int) level;
}

static void battery_task_entry(void *arg) {
    ESP_ERROR_CHECK(common_init_nvs());

    esp_err_t err;

    load_battery_curve();

    //-------------ADC1 Init---------------//
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN, &config));

    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_handle = NULL;
    bool do_calibration1 = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_handle);

    int8_t before_level, current_level;
    while (1) {
        adc_power_on_off(1);
        vTaskDelay(pdMS_TO_TICKS(5));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN, &_adc_raw));
        adc_power_on_off(0);

        if (do_calibration1) {
            int voltage;
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, _adc_raw, &voltage));
            _pre_pre_voltage = _pre_voltage;
            _pre_voltage = _voltage;

            before_level = battery_get_level();
            _voltage = voltage;
            current_level = battery_get_level();

            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV level:%d", ADC_UNIT_1 + 1, ADC1_CHAN, voltage, current_level);
            if (current_level != before_level) {
                // battery level change
                common_post_event_data(BIKE_BATTERY_EVENT,
                                       BATTERY_LEVEL_CHANGE,
                                       &current_level,
                                       sizeof(int8_t));
            }

            if (start_battery_curve) {
                err = add_battery_curve(voltage);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "add battery curve failed %d %s", err, esp_err_to_name(err));
                } else {
                    ESP_LOGI(TAG, "add battery curve %d", voltage);
                }
            }
        } else {
            ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC1_CHAN, _adc_raw);
        }
        vTaskDelay(pdMS_TO_TICKS(180000));
    }

    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (do_calibration1) {
        adc_calibration_deinit(adc1_cali_handle);
    }
}

void battery_init(void) {
    init_power_gpio();
    adc_power_on_off(0);

    /* Create battery task */
    BaseType_t err = xTaskCreate(
            battery_task_entry,
            "battery",
            2048,
            NULL,
            uxTaskPriorityGet(NULL),
            &battery_tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create battery detect task failed");
    }

    ESP_LOGI(TAG, "battery task init OK");
}


/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = unit,
                .atten = atten,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle) {
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
}

// returen mv
int battery_get_voltage() {
    return _voltage;
}

int8_t battery_get_level() {
    // invalid
    if (_voltage < 1000) {
        return -1;
    }

    uint32_t curve_count = battery_curve_size / sizeof(uint32_t);
    if (curve_count <= 5 || battery_is_curving()) {
        curve_count = sizeof(default_battery_curve_data) / sizeof(uint32_t);
        if (_voltage >= default_battery_curve_data[0]) {
            return 100;
        } else if (_voltage <= default_battery_curve_data[curve_count - 1]) {
            return 0;
        }
        float gap_size = 100.0f / ((float) curve_count - 1);
        for (int i = 0; i < curve_count; i++) {
            if (_voltage >= default_battery_curve_data[i]) {
                if (i == 0) {
                    return 100;
                }

                uint32_t pre = default_battery_curve_data[i - 1];
                uint32_t aft = default_battery_curve_data[i];

                float level = 100.0f - (float) i * gap_size -
                              ((float) pre - (float) _voltage) / ((float) pre - (float) aft) * gap_size;
                return (int) level;
            }
        }

        return 0;
    }
    return battery_voltage_to_level(_voltage);
}

void battery_deinit() {
    if (battery_tsk_hdl) {
        vTaskDelete(battery_tsk_hdl);
    }

    adc_power_on_off(0);
}

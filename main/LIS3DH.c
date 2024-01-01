//
// Created by yang on 2023/4/9.
//

#include "LIS3DH.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "common_utils.h"

#define TAG "LIS3DH"

ESP_EVENT_DEFINE_BASE(BIKE_MOTION_EVENT);
#define I2C_MASTER_TIMEOUT_MS       500

// https://learn.adafruit.com/adafruit-lis3dh-triple-axis-accelerometer-breakout/arduino
// https://github.com/adafruit/Adafruit_LIS3DH/tree/master
// https://github.com/ldab/lis3dh-motion-detection
// https://github.com/electricimp/LIS3DH
// Adjust this number for the sensitivity of the 'click' force
// this strongly depend on the range! for 16G, try 5-10
// for 8G, try 10-20. for 4G try 20-40. for 2G try 40-80
#define CLICKTHRESHHOLD 100

static TaskHandle_t imu_tsk_hdl = NULL;
static QueueHandle_t imu_int_event_queue;
static lis3dh_mode_t _mode;
static lis3dh_acc_range_t _acc_range;
static lis3dh_acc_sample_rage_t _acc_sample_rate;

extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;

static esp_err_t i2c_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1,
                                       data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t i2c_write_byte(uint8_t reg_addr, uint8_t data) {
    esp_err_t ret;
    uint8_t write_buf[2] = {reg_addr, data};
    ret = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    return ret;
}

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    gpio_num_t gpio_num = (gpio_num_t) arg;
    xQueueSendFromISR(imu_int_event_queue, &gpio_num, NULL);
    // ESP_LOGI(TAG, "imu isr triggered %d", gpio_num);
}

static void imu_task_entry(void *arg) {
    gpio_num_t triggered_gpio;
    while (1) {
        if (xQueueReceive(imu_int_event_queue, &triggered_gpio, portMAX_DELAY)) {
            int level = gpio_get_level(triggered_gpio);
            ESP_LOGI(TAG, "imu isr event io:%d level:%d", triggered_gpio, level);
            if (triggered_gpio == IMU_INT_1_GPIO) {
                uint8_t has_int1;
                lis3dh_get_int1_src(&has_int1);

                common_post_event_data(BIKE_MOTION_EVENT,
                                       LIS3DH_ACC_EVENT_MOTION,
                                       &triggered_gpio,
                                       sizeof(triggered_gpio));
            } else {
                uint8_t single_click, double_click;
                lis3dh_get_click_src(&single_click, &double_click);

                common_post_event_data(BIKE_MOTION_EVENT,
                                       LIS3DH_ACC_EVENT_MOTION2,
                                       &triggered_gpio,
                                       sizeof(triggered_gpio));
            }
        }
    }
    vTaskDelete(NULL);
}

// must be 0x33
uint8_t lis3dh_who_am_i() {
    uint8_t d[1] = {0};
    i2c_read_reg(LIS3DH_REG_WHO_AM_I, d, 1);
    return d[0];
}

esp_err_t lis3dh_init(lis3dh_mode_t mode, lis3dh_acc_range_t acc_range, lis3dh_acc_sample_rage_t acc_sample_rate) {
    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = LIS3DH_ADDR,
            .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

    uint8_t who_am_i = lis3dh_who_am_i();
    if (who_am_i != 0x33) {
        return ESP_FAIL;
    }

    _mode = mode;
    lis3dh_set_sample_rate(acc_sample_rate);

    //Build CTRL_REG4
    uint8_t data = 0;

    //set high resolution
    if (mode == LIS3DH_HIGH_RES_MODE) {
        data |= 0x08;
    }

    //  Convert scaling
    switch (acc_range) {
        default:
        case LIS3DH_ACC_RANGE_2:
            data |= (0x00 << 4);
            break;
        case LIS3DH_ACC_RANGE_4:
            data |= (0x01 << 4);
            break;
        case LIS3DH_ACC_RANGE_8:
            data |= (0x02 << 4);
            break;
        case LIS3DH_ACC_RANGE_16:
            data |= (0x03 << 4);
            break;
    }

    ESP_LOGI(TAG, "write to CTRL_REG4 %x", data);
    i2c_write_byte(LIS3DH_REG_CTRL_REG4, data);

    _mode = mode;
    _acc_range = acc_range;
    _acc_sample_rate = acc_sample_rate;
    return ESP_OK;
}

esp_err_t lis3dh_deinit() {
    // shutdown
    lis3dh_shutdown();

    if (imu_tsk_hdl != NULL) {
        vTaskDelete(imu_tsk_hdl);
        imu_tsk_hdl = NULL;
    }
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t lis3dh_current_mode(lis3dh_mode_t *mode, lis3dh_acc_range_t *acc_range,
                              lis3dh_acc_sample_rage_t *acc_sample_rate) {
    // CTRL_REG1[3] LOW POWER EN
    // CTRL_REG4[3] HIGH RES EN
    uint8_t d[1] = {0};
    i2c_read_reg(LIS3DH_REG_CTRL_REG1, d, 1);
    d[0] &= 0x0f; // set sample rate to 0

    *acc_sample_rate = d[0] >> 4;
    bool low_power = d[0] >> 3 & 0x01;

    d[0] = 0;
    i2c_read_reg(LIS3DH_REG_CTRL_REG4, d, 1);

    if (low_power) {
        *mode = LIS3DH_LOW_POWER_MODE;
    } else {
        bool high_res = d[0] >> 3 & 0x01;
        if (high_res) {
            *mode = LIS3DH_HIGH_RES_MODE;
        } else {
            *mode = LIS3DH_NORMAL_MODE;
        }
    }
    *acc_range = (d[0] >> 4 & 0x03);
    return ESP_OK;
}

esp_err_t lis3dh_set_sample_rate(lis3dh_acc_sample_rage_t acc_sample_rate) {
    uint8_t data = LIS3DH_ACC_SAMPLE_RATE_0;
    //Build CTRL_REG1
    // page 16 set CTRL_REG1[3](LPen bit)
    if (_mode == LIS3DH_LOW_POWER_MODE) {
        data |= 0x08;
    }

    //  Convert ODR
    switch (acc_sample_rate) {
        case LIS3DH_ACC_SAMPLE_RATE_1:
            data |= (0x01 << 4);
            break;
        case LIS3DH_ACC_SAMPLE_RATE_10:
            data |= (0x02 << 4);
            break;
        case LIS3DH_ACC_SAMPLE_RATE_25:
            data |= (0x03 << 4);
            break;
        case LIS3DH_ACC_SAMPLE_RATE_50:
            data |= (0x04 << 4);
            break;
        case LIS3DH_ACC_SAMPLE_RATE_100:
            data |= (0x05 << 4);
            break;
        case LIS3DH_ACC_SAMPLE_RATE_200:
            data |= (0x06 << 4);
            break;
        case LIS3DH_ACC_SAMPLE_RATE_400:
            data |= (0x07 << 4);
            break;
        default:
            break;
    }

    // en accx y z
    data |= 0x07;
    ESP_LOGI(TAG, "write to CTRL_REG1 %x", data);
    i2c_write_byte(LIS3DH_REG_CTRL_REG1, data);

    _acc_sample_rate = acc_sample_rate;

    return ESP_OK;
}

esp_err_t lis3dh_shutdown() {
    return lis3dh_set_sample_rate(LIS3DH_ACC_SAMPLE_RATE_0);
}

uint8_t lis3dh_read_status() {
    uint8_t d[1] = {0};
    i2c_read_reg(LIS3DH_REG_STATUS, d, 1);
    return d[0];
}

esp_err_t lis3dh_read_acc(float *accx, float *accy, float *accz) {
    uint8_t acc_data[6] = {0};

    uint8_t addr = LIS3DH_REG_OUT_X_L | 0x80; // auto incr addr
    esp_err_t err = i2c_read_reg(addr, acc_data, 6);
    if (err != ESP_OK) {
        return err;
    }

    int16_t *r_acc_data = (int16_t *) acc_data;
    int16_t data_raw_x = r_acc_data[0];
    int16_t data_raw_y = r_acc_data[1];
    int16_t data_raw_z = r_acc_data[2];
    ESP_LOGI(TAG, "read byte %d %d %d range:%d", data_raw_x, data_raw_y, data_raw_z, _acc_range);

    // regardless of the range, we'll always convert the value to 10 bits and g's
    // so we'll always divide by LIS3DH_LSB16_TO_KILO_LSB10 (16000):

    // then we can then multiply the resulting value by the lsb value to get the
    // value in g's

    uint8_t lsb_value = 1;
    if (_acc_range == LIS3DH_ACC_RANGE_2) {
        lsb_value = 4;
    } else if (_acc_range == LIS3DH_ACC_RANGE_4) {
        lsb_value = 8;
    } else if (_acc_range == LIS3DH_ACC_RANGE_8) {
        lsb_value = 16;
    } else if (_acc_range == LIS3DH_ACC_RANGE_16) {
        lsb_value = 48;
    }

    *accx = lsb_value * ((float) data_raw_x / LIS3DH_LSB16_TO_KILO_LSB10);
    *accy = lsb_value * ((float) data_raw_y / LIS3DH_LSB16_TO_KILO_LSB10);
    *accz = lsb_value * ((float) data_raw_z / LIS3DH_LSB16_TO_KILO_LSB10);

    return ESP_OK;
}

uint8_t lis3dh_config_motion_detect() {
    // INT1 config
    // for motion detect
    uint8_t d = 0;

    // // high pass filter logic
    // LIS3DH_HPF_AOI_INT1, LIS3DH_HPF_CUTOFF1, LIS3DH_HPF_NORMAL_MODE
    i2c_write_byte(LIS3DH_REG_CTRL_REG2, LIS3DH_HPF_AOI_INT1 | LIS3DH_HPF_CUTOFF1 | LIS3DH_HPF_NORMAL_MODE);

    // INT1 AND INT2 Latch
    d = 0b00001010;
    i2c_write_byte(LIS3DH_REG_CTRL_REG5, d);

    // 0x7f = max 0~127 -> 0 ~ range
    d = 10;
    i2c_write_byte(LIS3DH_REG_INT1_THS, d);

    // two intr min
    // x / sample rate // eg 25hz, x = 10  / 25 = 500ms
    d = 10;
    i2c_write_byte(LIS3DH_REG_INT1_DURATION, d);

    d = 0b00101010;
    i2c_write_byte(LIS3DH_REG_INT1_CFG, d);

    // IA1 enable on int1
    d = 0b01000000;
    i2c_write_byte(LIS3DH_REG_CTRL_REG3, d);


    // INT2 config
    // for click detect
    d = 0x15; // single click or 0x2a double click
    i2c_write_byte(LIS3DH_REG_CLICK_CFG, d);

    // 0 ~ 127
    i2c_write_byte(LIS3DH_REG_CLICK_THS, CLICKTHRESHHOLD);

    // Time acceleration has to fall below threshold for a valid click.
    // 2 / 25hz = 100ms
    i2c_write_byte(LIS3DH_REG_TIME_LIMIT, 2);

    // hold-off time before allowing detection after click event
    // 2 / 10hz = 100ms
    i2c_write_byte(LIS3DH_REG_TIME_LATENCY, 2);

    // hold-off time before allowing detection after click event
    // 10 / 25hz = 500ms
    i2c_write_byte(LIS3DH_REG_TIME_WINDOW, 10);

    // en I2_CLICK Click interrupt on INT2
    d = 0x00 | (0x01 << 7);
    i2c_write_byte(LIS3DH_REG_CTRL_REG6, d);


    imu_int_event_queue = xQueueCreate(10, sizeof(gpio_num_t));
    // INT GPIO
    // Default: push-pull output forced to GND
    // high trigger
    gpio_config_t io_config = {
            .pin_bit_mask = (1ull << IMU_INT_1_GPIO) | (1ull << IMU_INT_2_GPIO),
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_POSEDGE,
            .pull_up_en = 0,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    ESP_LOGI(TAG, "imu int1 io %d, level %d", IMU_INT_1_GPIO, gpio_get_level(IMU_INT_1_GPIO));
    uint8_t has_int1;
    lis3dh_get_int1_src(&has_int1);
    ESP_LOGI(TAG, "imu int2 io %d, level %d", IMU_INT_2_GPIO, gpio_get_level(IMU_INT_2_GPIO));
    uint8_t single_click, double_click;
    lis3dh_get_click_src(&single_click, &double_click);

    //install gpio isr service
    gpio_install_isr_service(0);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(IMU_INT_1_GPIO, gpio_isr_handler, (void *) IMU_INT_1_GPIO);
    gpio_isr_handler_add(IMU_INT_2_GPIO, gpio_isr_handler, (void *) IMU_INT_2_GPIO);
    ESP_LOGI(TAG, "imu motion detect isr add OK");

    /* Create key click detect task */
    BaseType_t err = xTaskCreate(
            imu_task_entry,
            "imu_task",
            2048,
            NULL,
            uxTaskPriorityGet(NULL),
            &imu_tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create imu int task failed");
    }
    ESP_LOGI(TAG, "imu motion detect task create success!");
    return ESP_OK;
}

esp_err_t lis3dh_get_click_src(uint8_t *single_click, uint8_t *double_click) {
    uint8_t click_src;
    esp_err_t err = i2c_read_reg(LIS3DH_REG_CLICK_SRC, &click_src, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (!click_src) {
        return ESP_OK;
    }

    // 0b00110000
    if (!(click_src & 0x30)) {
        // no click detect
        return ESP_OK;
    }

    *single_click = (click_src & 0x10) ? 1 : 0;
    *double_click = (click_src & 0x20) ? 1 : 0;

    ESP_LOGI(TAG, "click detect single:%d double:%d", *single_click, *double_click);
    return ESP_OK;
}


esp_err_t lis3dh_get_int1_src(uint8_t *has_int) {
    uint8_t int1_src;
    esp_err_t err = i2c_read_reg(LIS3DH_REG_INT1_SRC, &int1_src, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (!int1_src) {
        return ESP_OK;
    }

    *has_int = 1;

    ESP_LOGI(TAG, "int1 src: %x", int1_src);
    return ESP_OK;
}
//
// Created by yang on 2023/4/9.
//

#include "LIS3DH.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "common_utils.h"

#define TAG "LIS3DH"

ESP_EVENT_DEFINE_BASE(BIKE_MOTION_EVENT);
#define I2C_MASTER_TIMEOUT_MS       100

// https://learn.adafruit.com/adafruit-lis3dh-triple-axis-accelerometer-breakout/arduino
// https://github.com/adafruit/Adafruit_LIS3DH/tree/master
// https://github.com/ldab/lis3dh-motion-detection
// https://github.com/electricimp/LIS3DH
// Adjust this number for the sensitivity of the 'click' force
// this strongly depend on the range! for 16G, try 5-10
// for 8G, try 10-20. for 4G try 20-40. for 2G try 40-80
#define CLICKTHRESHHOLD 100
#define ACC_GRAVITY (0.98f)

// 0011001 sa0 = 1 , default pull up
// 0011000 sa0 = 0
#define LIS3DH_ADDR 0B0011001

#define LIS3DH_REG_STATUS 0x07

// 0x33
#define LIS3DH_REG_WHO_AM_I 0x0F

#define LIS3DH_REG_CTRL_REG0 0x1E
#define LIS3DH_REG_CTRL_REG1 0x20
#define LIS3DH_REG_CTRL_REG2 0x21
#define LIS3DH_REG_CTRL_REG3 0x22
#define LIS3DH_REG_CTRL_REG4 0x23
#define LIS3DH_REG_CTRL_REG5 0x24
#define LIS3DH_REG_CTRL_REG6 0x25

#define LIS3DH_REG_OUT_X_L 0x28
#define LIS3DH_REG_OUT_X_H 0x29
#define LIS3DH_REG_OUT_Y_L 0x2A
#define LIS3DH_REG_OUT_Y_H 0x2B
#define LIS3DH_REG_OUT_Z_L 0x2C
#define LIS3DH_REG_OUT_Z_H 0x2D

#define LIS3DH_REG_INT1_CFG 0x30
#define LIS3DH_REG_INT1_SRC 0x31
#define LIS3DH_REG_INT1_THS 0x32
#define LIS3DH_REG_INT1_DURATION 0x33

#define LIS3DH_REG_INT2_CFG 0x34
#define LIS3DH_REG_INT2_SRC 0x35
#define LIS3DH_REG_INT2_THS 0x36
#define LIS3DH_REG_INT2_DURATION 0x37

#define LIS3DH_REG_CLICK_CFG 0x38
#define LIS3DH_REG_CLICK_SRC 0x39
#define LIS3DH_REG_CLICK_THS 0x3A
#define LIS3DH_REG_TIME_LIMIT 0x3B
#define LIS3DH_REG_TIME_LATENCY 0x3C
#define LIS3DH_REG_TIME_WINDOW 0x3D


// High Pass Filter values
#define LIS3DH_HPF_DISABLED               0x00
#define LIS3DH_HPF_AOI_INT1               0x01
#define LIS3DH_HPF_AOI_INT2               0x02
#define LIS3DH_HPF_CLICK                  0x04
#define LIS3DH_HPF_FDS                    0x08

#define LIS3DH_HPF_CUTOFF1                0x00
#define LIS3DH_HPF_CUTOFF2                0x10
#define LIS3DH_HPF_CUTOFF3                0x20
#define LIS3DH_HPF_CUTOFF4                0x30

#define LIS3DH_HPF_DEFAULT_MODE           0x00
#define LIS3DH_HPF_REFERENCE_SIGNAL       0x40
#define LIS3DH_HPF_NORMAL_MODE            0x80
#define LIS3DH_HPF_AUTORESET_ON_INTERRUPT 0xC0


///< Scalar to convert from 16-bit lsb to 10-bit and divide by 1k to
///< convert from milli-gs to gs
#define LIS3DH_LSB16_TO_KILO_LSB10  64000

static bool lis3dh_inited = false;
static TaskHandle_t imu_tsk_hdl = NULL;
static QueueHandle_t imu_int_event_queue;

RTC_DATA_ATTR bool _acc_who_ami_checked = false;
RTC_DATA_ATTR lis3dh_mode_t _acc_mode = LIS3DH_NORMAL_MODE;
RTC_DATA_ATTR lis3dh_acc_range_t _acc_range = LIS3DH_ACC_RANGE_2;
RTC_DATA_ATTR lis3dh_acc_sample_rage_t _acc_sample_rate = LIS3DH_ACC_SAMPLE_RATE_0;
RTC_DATA_ATTR lis3dh_direction_t lis3dsh_direction = LIS3DH_DIR_TOP;
RTC_DATA_ATTR bool _acc_motion_detect_sensor_inited = false;

extern i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t dev_handle;

static esp_err_t i2c_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1,
                                       data, len, I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t i2c_write_byte(uint8_t reg_addr, uint8_t data) {
    esp_err_t ret;
    uint8_t write_buf[2] = {reg_addr, data};
    ret = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
    return ret;
}

#if IMU_INT_1_GPIO >= 0
static void IRAM_ATTR mpu_gpio_isr_handler(void *arg) {
    gpio_num_t gpio_num = (gpio_num_t) arg;
    xQueueSendFromISR(imu_int_event_queue, &gpio_num, NULL);
    // ESP_LOGI(TAG, "imu isr triggered %d", gpio_num);
}
#endif

static void imu_task_entry(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(300));

    if (!_acc_motion_detect_sensor_inited) {
        ESP_LOGI(TAG, "config for motion and click detect");
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
        d = 6;
        i2c_write_byte(LIS3DH_REG_INT1_THS, d);

        // two intr min
        // x / sample rate // eg 25hz, x = 10 / 25 = 400ms
        d = 10;
        i2c_write_byte(LIS3DH_REG_INT1_DURATION, d);

        d = 0b00101010;
        // 00 OR combination of interrupt events
        // en z high low 10
        // en y high low 10
        // en x high low 10
        i2c_write_byte(LIS3DH_REG_INT1_CFG, d);

        // IA1 enable on int1
        d = 0b01000000;
        i2c_write_byte(LIS3DH_REG_CTRL_REG3, d);

#if ENABLE_INT2
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
#else
        d = 0x00 | (IMU_INT_ACTIVE_LEVEL ? 0 : 1) << 1;
        i2c_write_byte(LIS3DH_REG_CTRL_REG6, d);
#endif

        _acc_motion_detect_sensor_inited = true;
        ESP_LOGI(TAG, "motion detect init success");
    }

    imu_int_event_queue = xQueueCreate(6, sizeof(gpio_num_t));
#if IMU_INT_1_GPIO >= 0
    // INT GPIO
    // Default: push-pull output forced to GND
    // high trigger
    gpio_config_t io_config = {
            .pin_bit_mask = (1ull << IMU_INT_1_GPIO),
            .mode = GPIO_MODE_INPUT,
            .intr_type = IMU_INT_ACTIVE_LEVEL ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
            .pull_up_en = 0,
            .pull_down_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    if (gpio_get_level(IMU_INT_1_GPIO) == IMU_INT_ACTIVE_LEVEL) {
        uint8_t has_int1;
        lis3dh_get_int1_src(&has_int1);

        ESP_LOGI(TAG, "imu int1 io %d, level %d", IMU_INT_1_GPIO, gpio_get_level(IMU_INT_1_GPIO));
    }

    //install gpio isr service
    //gpio_install_isr_service(0);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(IMU_INT_1_GPIO, mpu_gpio_isr_handler, (void *) IMU_INT_1_GPIO);
    ESP_LOGI(TAG, "imu motion detect isr add OK");
#endif

    gpio_num_t triggered_gpio;
    uint8_t continue_timeout_count = 1;

    while (1) {
        uint32_t sleep_ms = min(10000, continue_timeout_count * 1000);
        bool int_triggered = xQueueReceive(imu_int_event_queue, &triggered_gpio, pdMS_TO_TICKS(sleep_ms));
#if IMU_INT_1_GPIO >= 0
        int level = gpio_get_level(triggered_gpio);
        ESP_LOGI(TAG, "imu isr event io:%d level:%d", triggered_gpio, level);
        gpio_intr_disable(triggered_gpio);

        if (int_triggered && triggered_gpio == IMU_INT_1_GPIO) {
#endif
        uint8_t has_int1 = 0;
        if (int_triggered || IMU_INT_1_GPIO < 0) {
            lis3dh_get_int1_src(&has_int1);

            if (has_int1) {
                common_post_event(BIKE_MOTION_EVENT, LIS3DH_ACC_EVENT_MOTION);
            }
        }

#if IMU_INT_1_GPIO >= 0
        }
#endif
#if ENABLE_INT2
        else {
            uint8_t single_click, double_click;
            lis3dh_get_click_src(&single_click, &double_click);

            common_post_event(BIKE_MOTION_EVENT, LIS3DH_ACC_EVENT_MOTION2);
        }
#endif
#if IMU_INT_1_GPIO >= 0
        gpio_intr_enable(triggered_gpio);
#endif
        if (!has_int1) {
            // time out read acc sensor to calc display direction
            lis3dh_direction_t direction = lis3dh_calc_direction();
            ESP_LOGD(TAG, "calc display rotation %d", direction);
            if (direction != LIS3DH_DIR_UNKNOWN && direction != lis3dsh_direction) {
                // direction change
                ESP_LOGI(TAG, "direction change to %d", direction);
                lis3dsh_direction = direction;
                common_post_event_data(BIKE_MOTION_EVENT,
                                       LIS3DH_DIRECTION_CHANGE,
                                       &lis3dsh_direction,
                                       sizeof(lis3dsh_direction));
            }
            continue_timeout_count++;
        } else {
            continue_timeout_count = 0;
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
    if (lis3dh_inited) {
        return ESP_OK;
    }
    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = LIS3DH_ADDR,
            .scl_speed_hz = 200000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

    if (!_acc_who_ami_checked) {
        uint8_t who_am_i = lis3dh_who_am_i();
        if (who_am_i != 0x33) {
            lis3dh_inited = false;
            ESP_LOGE(TAG, "who ami check failed expect 0x33 but %x", who_am_i);
            return ESP_FAIL;
        }
        _acc_who_ami_checked = true;
    }

    lis3dh_set_sample_rate(mode == LIS3DH_LOW_POWER_MODE, acc_sample_rate);
    lis3dh_set_acc_range(mode == LIS3DH_HIGH_RES_MODE, acc_range);

    lis3dh_inited = true;
    ESP_LOGI(TAG, "imu inited!");
    return ESP_OK;
}

esp_err_t lis3dh_deinit() {
    // shutdown
    if (!lis3dh_inited) {
        return ESP_OK;
    }

    if (imu_tsk_hdl != NULL) {
        vTaskDelete(imu_tsk_hdl);
        imu_tsk_hdl = NULL;
    }

    lis3dh_shutdown();

    esp_err_t err = ESP_OK;
    if (dev_handle != NULL) {
        err = i2c_master_bus_rm_device(dev_handle);
        dev_handle = NULL;
    }
    lis3dh_inited = false;
    return err;
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

esp_err_t lis3dh_set_acc_range(bool high_res, lis3dh_acc_range_t acc_range) {
    bool hr = _acc_mode == LIS3DH_HIGH_RES_MODE;
    if (hr == high_res && _acc_range == acc_range) {
        return ESP_OK;
    }

    uint8_t data = 0;
    //set high resolution
    if (high_res) {
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
    esp_err_t err = i2c_write_byte(LIS3DH_REG_CTRL_REG4, data);
    if (err == ESP_OK) {
        _acc_range = acc_range;
        if (high_res) {
            _acc_mode = LIS3DH_HIGH_RES_MODE;
        }
    }

    return err;
}

esp_err_t lis3dh_set_sample_rate(bool low_power, lis3dh_acc_sample_rage_t acc_sample_rate) {
    bool lp = _acc_mode == LIS3DH_LOW_POWER_MODE;
    if (lp == low_power && acc_sample_rate == _acc_sample_rate) {
        return ESP_OK;
    }

    uint8_t data = LIS3DH_ACC_SAMPLE_RATE_0;
    //Build CTRL_REG1
    // page 16 set CTRL_REG1[3](LPen bit)
    if (low_power) {
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
    esp_err_t err = i2c_write_byte(LIS3DH_REG_CTRL_REG1, data);
    if (err == ESP_OK) {
        _acc_sample_rate = acc_sample_rate;
        if (low_power) {
            _acc_mode = LIS3DH_LOW_POWER_MODE;
        }
    }
    return err;
}

esp_err_t lis3dh_shutdown() {
    if (!lis3dh_inited) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "shutdown lis3dh");
    return lis3dh_set_sample_rate(true, LIS3DH_ACC_SAMPLE_RATE_0);
}

uint8_t lis3dh_read_status() {
    uint8_t d[1] = {0};
    i2c_read_reg(LIS3DH_REG_STATUS, d, 1);
    return d[0];
}

esp_err_t lis3dh_read_acc(float *accx, float *accy, float *accz) {
    if (!lis3dh_inited) {
        return ESP_FAIL;
    }

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
    //ESP_LOGI(TAG, "read byte %d %d %d range:%d", data_raw_x, data_raw_y, data_raw_z, _acc_range);

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

lis3dh_direction_t lis3dh_calc_direction() {
    if (!lis3dh_inited) {
        return LIS3DH_DIR_UNKNOWN;
    }

    float accx, accy, accz;
    float threshold = 0.15f;
    esp_err_t err = lis3dh_read_acc(&accx, &accy, &accz);
    if (err != ESP_OK) {
        return LIS3DH_DIR_UNKNOWN;
    }

    if (mabs(accx) < threshold && accy < -ACC_GRAVITY + threshold && mabs(accz) < threshold) {
        return LIS3DH_DIR_BOTTOM;
    }
    if (accx >= ACC_GRAVITY - threshold && mabs(accy) < threshold && mabs(accz) < threshold) {
        return LIS3DH_DIR_RIGHT;
    }

    if (mabs(accx) < threshold && accy > ACC_GRAVITY - threshold && mabs(accz) < threshold) {
        return LIS3DH_DIR_TOP;
    }

    if (accx < -ACC_GRAVITY + threshold && mabs(accy) < threshold && mabs(accz) < threshold) {
        return LIS3DH_DIR_LEFT;
    }

    return LIS3DH_DIR_UNKNOWN;
}

lis3dh_direction_t lis3dh_get_direction() {
    return lis3dsh_direction;
}

uint8_t lis3dh_config_motion_detect() {
    if (imu_tsk_hdl != NULL) {
        return ESP_OK;
    }

    /* Create key click detect task */
    BaseType_t err = xTaskCreate(
            imu_task_entry,
            "imu_task",
            3072,
            NULL,
            4,
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
    *has_int = (int1_src >> 6) & 0x01;
    ESP_LOGI(TAG, "int1 src: %x has int:%d", int1_src, *has_int);
    return ESP_OK;
}

esp_err_t lis3dh_disable_int() {
    if (!lis3dh_inited) {
        return ESP_FAIL;
    }
    uint8_t d = 0x00;
    return i2c_write_byte(LIS3DH_REG_CTRL_REG3, d);
}
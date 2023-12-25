//
// Created by yang on 2023/12/19.
//

#include "driver/ledc.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "musical_score_encoder.h"
#include "beep.h"

#define TAG "beep"

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

// rmt mode
#define RMT_BUZZER_RESOLUTION_HZ 1000000 // 1MHz resolution

static beep_mode_t beep_mode;
rmt_channel_handle_t buzzer_chan = NULL;
rmt_encoder_handle_t score_encoder = NULL;

esp_err_t beep_init_pwm_mode() {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_MODE,
            .duty_resolution  = LEDC_DUTY_RES,
            .timer_num        = LEDC_TIMER,
            .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
            .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
            .speed_mode     = LEDC_MODE,
            .channel        = LEDC_CHANNEL,
            .timer_sel      = LEDC_TIMER,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = BEEP_GPIO_NUM,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0
    };
    esp_err_t err = ledc_channel_config(&ledc_channel);

    return err;
}

esp_err_t beep_init_rmt_mode() {
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
            .gpio_num = BEEP_GPIO_NUM,
            .mem_block_symbols = 64,
            .resolution_hz = RMT_BUZZER_RESOLUTION_HZ,
            .trans_queue_depth = 10, // set the maximum number of transactions that can pend in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &buzzer_chan));

    ESP_LOGI(TAG, "Install musical score encoder");
    musical_score_encoder_config_t encoder_config = {
            .resolution = RMT_BUZZER_RESOLUTION_HZ
    };
    ESP_ERROR_CHECK(rmt_new_musical_score_encoder(&encoder_config, &score_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(buzzer_chan));
    return ESP_OK;
}

esp_err_t beep_init(beep_mode_t mode) {
    beep_mode = mode;
    esp_err_t err;
    if (mode == BEEP_MODE_PWM) {
        err = beep_init_pwm_mode();
    } else {
        err = beep_init_rmt_mode();
    }
    return err;
}

/**
 *  duration ms if = 0 no stop beep
 */
esp_err_t beep_start_beep(uint32_t duration) {
    if (beep_mode == BEEP_MODE_PWM) {
        // Set duty to 50%
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
        // Update duty to apply the new value
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

        esp_err_t err = ledc_timer_resume(LEDC_MODE, LEDC_TIMER);
        return err;
    } else {
        ESP_ERROR_CHECK(rmt_enable(buzzer_chan));
        const buzzer_musical_score_t beep_score[] = {{.freq_hz = 4000, .duration_ms = duration}};
        for (size_t i = 0; i < sizeof(beep_score) / sizeof(beep_score[0]); i++) {
            rmt_transmit_config_t tx_config = {
                    .loop_count = beep_score[i].duration_ms * beep_score[i].freq_hz / 1000,
            };
            if (beep_score[i].freq_hz == 0) {
                tx_config.loop_count = 1;
            }
            ESP_ERROR_CHECK(rmt_transmit(buzzer_chan, score_encoder, &beep_score[i], sizeof(buzzer_musical_score_t),
                                         &tx_config));
        }
    }
    return ESP_OK;
}

esp_err_t beep_start_play(const buzzer_musical_score_t *song, uint16_t song_len) {
    if (beep_mode == BEEP_MODE_PWM) {
        // TODO unsupported
    } else {
        //ESP_ERROR_CHECK(rmt_enable(buzzer_chan));
        ESP_LOGI(TAG, "song length %d", song_len);
        for (size_t i = 0; i < song_len; i++) {
            rmt_transmit_config_t tx_config = {
                    .loop_count = song[i].duration_ms * song[i].freq_hz / 1000,
            };
            ESP_LOGI(TAG, "start play song index %d", i);
            if (song[i].freq_hz < 10) {
                vTaskDelay(pdMS_TO_TICKS(song[i].duration_ms));
            } else {
                ESP_ERROR_CHECK(
                        rmt_transmit(buzzer_chan, score_encoder, &song[i], sizeof(buzzer_musical_score_t), &tx_config));
                rmt_tx_wait_all_done(buzzer_chan, 5000);
            }
        }
    }
    return ESP_OK;
}

esp_err_t stop_beep() {
    if (beep_mode == BEEP_MODE_PWM) {
        // Set duty to 0%
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
        // Update duty to apply the new value
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

        esp_err_t err = ledc_timer_pause(LEDC_MODE, LEDC_TIMER);
        return err;
    } else {
        rmt_disable(buzzer_chan);
    }

    return ESP_OK;
}

esp_err_t beep_deinit() {
    stop_beep();

    if (beep_mode == BEEP_MODE_PWM) {

    }

    return ESP_OK;
}
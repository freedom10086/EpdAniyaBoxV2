//
// Created by yang on 2024/1/24.
//

#include "esp_log.h"

#include "tomato_clock.h"
#include "static/static.h"
#include "page_manager.h"
#include "confirm_menu_page.h"
#include "max31328.h"
#include "common_utils.h"
#include "beep/beep.h"

#define TAG "tomato-clock"

#define STUDY_TIME_MIN 5
#define STUDY_TIME_MAX 120

#define PLAY_TIME_MIN 1
#define PLAY_TIME_MAX 60

#define LOOP_COUNT_MAX 5

#define DEFAULT_STUDY_TIME 45
#define DEFAULT_PLAY_TIME 15
#define DEFAULT_LOOP_COUNT 2

#define TIMER_DELTA_TS 2

#define USE_FIX_TIMER_INTR 0

RTC_DATA_ATTR static uint8_t study_time_min = DEFAULT_STUDY_TIME;
RTC_DATA_ATTR static uint8_t play_time_min = DEFAULT_PLAY_TIME;
RTC_DATA_ATTR static uint8_t _loop_count = DEFAULT_LOOP_COUNT;

RTC_DATA_ATTR static tomato_stage_t curr_stage = TOMATO_INIT;
RTC_DATA_ATTR static uint8_t curr_loop = 1;

RTC_DATA_ATTR static time_t study_start_ts = 0;
RTC_DATA_ATTR static time_t play_start_ts = 0;

static esp_timer_handle_t timer_hdl = NULL;

static time_t _current_ts = 0;

static char buff[32];

static confirm_menu_arg_t confirm_menu_arg = {
        .title_label = "skip?",
        .callback = NULL
};

static void change_to_next_stage(bool refresh);

static bool auto_change_stage();

static void timer_callback(void *arg) {
    tomato_stage_t *timer_stage = (tomato_stage_t *) arg;
    ESP_LOGI(TAG, "rev timer callback %d", *timer_stage);
    if (*timer_stage != curr_stage) {
        // stage has changed ignore

    } else {
        change_to_next_stage(true);
    }
}

static void create_time_arrive_timer(bool check_current_ts) {
    if (curr_stage != TOMATO_STUDYING
        && curr_stage != TOMATO_PLAYING
        && curr_stage != TOMATO_SUMMARY) {
        return;
    }

    if (timer_hdl != NULL) {
        if (esp_timer_is_active(timer_hdl)) {
            esp_timer_stop(timer_hdl);
        }
        esp_timer_delete(timer_hdl);
        timer_hdl = NULL;
    }

    if (check_current_ts) {
        max31328_get_time_ts(&_current_ts);
        if (_current_ts == 0) {
            return;
        }
    }

    uint32_t timer_ts = 0;
    if (curr_stage == TOMATO_STUDYING) {
        timer_ts = max(0, study_start_ts + study_time_min * 60 - _current_ts);
        if (timer_ts <= TIMER_DELTA_TS) {
            return change_to_next_stage(true);
        }
    } else if (curr_stage == TOMATO_PLAYING) {
        timer_ts = max(0, play_start_ts + play_time_min * 60 - _current_ts);
        if (timer_ts <= TIMER_DELTA_TS) {
            return change_to_next_stage(true);
        }
    } else {
        // summary
        timer_ts = 60;
    }

    if (timer_ts > 0) {
        const esp_timer_create_args_t timer_args = {
                .callback = &timer_callback,
                /* argument specified here will be passed to timer callback function */
                .arg = &curr_stage,
                .name = "tomato-timer"
        };

        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_hdl));
        ESP_ERROR_CHECK(esp_timer_start_once(timer_hdl, (uint64_t) timer_ts * 1000 * 1000 + 500 * 1000));
        ESP_LOGI(TAG, "create timer ts:%ld", timer_ts);
    }
#if USE_FIX_TIMER_INTR
    if (timer_ts > 0 && (curr_stage == TOMATO_STUDYING || curr_stage == TOMATO_PLAYING)) {
        rx8025t_set_fixed_time_intr(1, 1, timer_ts);
    }
#endif
}

void tomato_page_on_create(void *arg) {

    create_time_arrive_timer(true);
}

static bool auto_change_stage() {
    if (curr_stage == TOMATO_STUDYING) {
        if (_current_ts + TIMER_DELTA_TS >= study_start_ts + study_time_min * 60) {
            ESP_LOGI(TAG, "change from study to next stage");
            change_to_next_stage(false);
            return true;
        }
    } else if (curr_stage == TOMATO_PLAYING) {
        if (_current_ts + TIMER_DELTA_TS >= play_start_ts + play_time_min * 60) {
            ESP_LOGI(TAG, "change from play to next stage");
            change_to_next_stage(false);
            return true;
        }
    }

    return false;
}

void tomato_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    if (loop_cnt > 1) {
        max31328_get_time_ts(&_current_ts);
    }

    auto_change_stage();

    epd_paint_draw_string_at_hposition(epd_paint, 0, 0, epd_paint->width, (char *) text_tomato_clock, &Font_HZK16,
                                       ALIGN_CENTER, 1);

    switch (curr_stage) {
        case TOMATO_INIT:
            epd_paint_draw_bitmap(epd_paint, (epd_paint->width - 32) / 2, 50, 32, 32, ic_tomato_bmp_start,
                                  ic_tomato_bmp_end - ic_tomato_bmp_start, 1);
            epd_paint_draw_string_at_hposition(epd_paint, 0, 140, epd_paint->width, (char *) text_start, &Font_HZK16,
                                               ALIGN_CENTER, 1);
            break;
        case TOMATO_SETTING_STUDY_TIME:
            epd_paint_draw_bitmap(epd_paint, (epd_paint->width - 32) / 2, 50, 32, 32, ic_tomato_bmp_start,
                                  ic_tomato_bmp_end - ic_tomato_bmp_start, 1);

            sprintf(buff, "%d", study_time_min);
            epd_paint_draw_string_at_hposition(epd_paint, 0, 120, epd_paint->width, buff,
                                               &Font_HZK16, ALIGN_CENTER, 1);

            epd_paint_draw_string_at_hposition(epd_paint, 0, 140, epd_paint->width, (char *) text_study_time,
                                               &Font_HZK16, ALIGN_CENTER, 1);
            break;
        case TOMATO_SETTING_PLAY_TIME:
            epd_paint_draw_bitmap(epd_paint, (epd_paint->width - 32) / 2, 50, 32, 32, ic_tomato_bmp_start,
                                  ic_tomato_bmp_end - ic_tomato_bmp_start, 1);

            sprintf(buff, "%d", play_time_min);
            epd_paint_draw_string_at_hposition(epd_paint, 0, 120, epd_paint->width, buff,
                                               &Font_HZK16, ALIGN_CENTER, 1);

            epd_paint_draw_string_at_hposition(epd_paint, 0, 140, epd_paint->width, (char *) text_play_time,
                                               &Font_HZK16, ALIGN_CENTER, 1);
            break;
        case TOMATO_SETTING_LOOP_COUNT:
            epd_paint_draw_bitmap(epd_paint, (epd_paint->width - 32) / 2, 50, 32, 32, ic_tomato_bmp_start,
                                  ic_tomato_bmp_end - ic_tomato_bmp_start, 1);

            sprintf(buff, "%d", _loop_count);
            epd_paint_draw_string_at_hposition(epd_paint, 0, 120, epd_paint->width, buff,
                                               &Font_HZK16, ALIGN_CENTER, 1);

            epd_paint_draw_string_at_hposition(epd_paint, 0, 140, epd_paint->width, (char *) text_loop_count,
                                               &Font_HZK16, ALIGN_CENTER, 1);
            break;
        case TOMATO_STUDYING:
            sprintf(buff, "%d/%d", curr_loop, _loop_count);
            epd_paint_draw_string_at(epd_paint, 0, 0, buff, &Font_HZK16, 1);

            epd_paint_draw_bitmap(epd_paint, (epd_paint->width - 32) / 2, 50, 32, 32, ic_studying_bmp_start,
                                  ic_studying_bmp_end - ic_studying_bmp_start, 1);
            uint8_t passed_study_min = min(study_time_min, (_current_ts - study_start_ts) / 60);
            sprintf(buff, "%d/%d", passed_study_min, study_time_min);
            epd_paint_draw_string_at_hposition(epd_paint, 0, 120, epd_paint->width, buff,
                                               &Font_HZK16, ALIGN_CENTER, 1);

            epd_paint_draw_string_at_hposition(epd_paint, 0, 140, epd_paint->width, (char *) text_studying, &Font_HZK16,
                                               ALIGN_CENTER, 1);
            break;
        case TOMATO_PLAYING:
            sprintf(buff, "%d/%d", curr_loop, _loop_count);
            epd_paint_draw_string_at(epd_paint, 0, 0, buff, &Font_HZK16, 1);

            epd_paint_draw_bitmap(epd_paint, (epd_paint->width - 32) / 2, 50, 32, 32, ic_playing_bmp_start,
                                  ic_playing_bmp_end - ic_playing_bmp_start, 1);
            uint8_t passed_play_min = min(play_time_min, (_current_ts - play_start_ts) / 60);
            sprintf(buff, "%d/%d", passed_play_min, play_time_min);
            epd_paint_draw_string_at_hposition(epd_paint, 0, 120, epd_paint->width, buff,
                                               &Font_HZK16, ALIGN_CENTER, 1);

            epd_paint_draw_string_at_hposition(epd_paint, 0, 140, epd_paint->width, (char *) text_playing, &Font_HZK16,
                                               ALIGN_CENTER, 1);
            break;
        case TOMATO_SUMMARY:
            epd_paint_draw_bitmap(epd_paint, (epd_paint->width - 32) / 2, 50, 32, 32, ic_summary_bmp_start,
                                  ic_summary_bmp_end - ic_summary_bmp_start, 1);
            epd_paint_draw_string_at_hposition(epd_paint, 0, 140, epd_paint->width, (char *) text_summary, &Font_HZK16,
                                               ALIGN_CENTER, 1);
            break;
        default:
            ESP_LOGW(TAG, "unknown stage %d", curr_stage);
            break;
    }
}

static void adjust_study_time(bool add) {
    if (add) {
        if (study_time_min >= 60) {
            study_time_min += 10;
        } else {
            study_time_min += 5;
        }
        if (study_time_min > STUDY_TIME_MAX) {
            study_time_min = STUDY_TIME_MAX;
        }
    } else {
        if (study_time_min >= 70) {
            study_time_min -= 10;
        } else {
            study_time_min -= 5;
        }

        if (study_time_min < STUDY_TIME_MIN) {
            study_time_min = STUDY_TIME_MIN;
        }
    }
    page_manager_request_update(false);
}

static void adjust_play_time(bool add) {
    if (add) {
        if (play_time_min >= 10) {
            play_time_min += 5;
        } else {
            play_time_min += 1;
        }
        if (play_time_min > PLAY_TIME_MAX) {
            play_time_min = PLAY_TIME_MAX;
        }
    } else {
        if (play_time_min >= 15) {
            play_time_min -= 5;
        } else {
            play_time_min -= 1;
        }

        if (play_time_min < PLAY_TIME_MIN) {
            play_time_min = PLAY_TIME_MIN;
        }
    }
    page_manager_request_update(false);
}

static void adjust_loop_count(bool add) {
    if (add) {
        if (_loop_count >= LOOP_COUNT_MAX) {
            return;
        }
        _loop_count++;
    } else {
        if (_loop_count <= 1) {
            return;
        }
        _loop_count--;
    }

    if (_loop_count < 1) {
        _loop_count = 1;
    }
    if (_loop_count > LOOP_COUNT_MAX) {
        _loop_count = LOOP_COUNT_MAX;
    }
    page_manager_request_update(false);
}

static void start_study() {
    if (curr_loop == 1) {
        //rx8025t_set_fixed_time_intr(0, 0, 60);
    }

    max31328_get_time_ts(&study_start_ts);
    ESP_LOGI(TAG, "start studying current ts %lld", study_start_ts);

    _current_ts = study_start_ts;
    create_time_arrive_timer(false);

    beep_init(BEEP_MODE_RMT);
    beep_start_play(music_score_beep, sizeof(music_score_beep) / sizeof(buzzer_musical_score_t));
    beep_deinit();
}

static void start_play() {
    max31328_get_time_ts(&play_start_ts);
    ESP_LOGI(TAG, "start playing current ts %lld", play_start_ts);

    _current_ts = play_start_ts;
    create_time_arrive_timer(false);

    beep_init(BEEP_MODE_RMT);
    beep_start_play(music_score_beep_beep, sizeof(music_score_beep_beep) / sizeof(buzzer_musical_score_t));
    beep_deinit();
}

static void summary() {
    // disable fix time timer
    create_time_arrive_timer(false);

    beep_init(BEEP_MODE_RMT);
    beep_start_play(music_score_beep_beep_beep, sizeof(music_score_beep_beep_beep) / sizeof(buzzer_musical_score_t));
    beep_deinit();
}

static void change_to_next_stage(bool refresh) {
    if (curr_stage == TOMATO_INIT) {
        curr_stage = TOMATO_SETTING_STUDY_TIME;
    } else if (curr_stage == TOMATO_SETTING_STUDY_TIME) {
        curr_stage = TOMATO_SETTING_PLAY_TIME;
    } else if (curr_stage == TOMATO_SETTING_PLAY_TIME) {
        curr_stage = TOMATO_SETTING_LOOP_COUNT;
    } else if (curr_stage == TOMATO_SETTING_LOOP_COUNT) {
        curr_stage = TOMATO_STUDYING;
        curr_loop = 1;
        start_study();
    } else if (curr_stage == TOMATO_STUDYING) {
        curr_stage = TOMATO_PLAYING;
        start_play();
    } else if (curr_stage == TOMATO_PLAYING) {
        // if has next loop
        if (curr_loop < _loop_count) {
            curr_stage = TOMATO_STUDYING;
            curr_loop++;
            start_study();
        } else {
            curr_stage = TOMATO_SUMMARY;
            summary();
        }
    } else if (curr_stage == TOMATO_SUMMARY) {
        curr_stage = TOMATO_INIT;
        curr_loop = 1;
    }

    if (refresh) {
        page_manager_request_update(false);
    }
}

static void confirm_skip_stage_callback(bool confirm) {
    if (confirm) {
        change_to_next_stage(false);
    }
}

bool tomato_page_key_click(key_event_id_t key_event_type) {
    switch (key_event_type) {
        case KEY_OK_SHORT_CLICK:
            if (curr_stage == TOMATO_STUDYING || curr_stage == TOMATO_PLAYING) {
                // skip
                confirm_menu_arg.callback = confirm_skip_stage_callback;
                page_manager_show_menu("confirm-alert", &confirm_menu_arg);
                page_manager_request_update(false);
            } else {
                // next stage
                change_to_next_stage(true);
            }
            return true;
        case KEY_UP_SHORT_CLICK:
        case KEY_DOWN_SHORT_CLICK:
            if (curr_stage == TOMATO_SETTING_STUDY_TIME) {
                adjust_study_time(key_event_type == (key_event_id_t) KEY_UP_SHORT_CLICK);
            } else if (curr_stage == TOMATO_SETTING_PLAY_TIME) {
                adjust_play_time(key_event_type == (key_event_id_t) KEY_UP_SHORT_CLICK);
            } else if (curr_stage == TOMATO_SETTING_LOOP_COUNT) {
                adjust_loop_count(key_event_type == (key_event_id_t) KEY_UP_SHORT_CLICK);
            }
            return true;
        default:
            break;
    }

    return false;
}

void tomato_page_on_destroy(void *arg) {
    if (timer_hdl != NULL) {
        if (esp_timer_is_active(timer_hdl)) {
            esp_timer_stop(timer_hdl);
        }
        esp_timer_delete(timer_hdl);
        timer_hdl = NULL;
    }

    if (curr_stage != TOMATO_INIT) {
        // disable fixtime timer
        //rx8025t_set_fixed_time_intr(0, 0, 60);
    }
}

int tomato_page_on_enter_sleep(void *arg) {
//    if (curr_stage == TOMATO_STUDYING || curr_stage == TOMATO_PLAYING) {
//        return 60;
//    }
//    return NO_SLEEP_TS;

    return NEVER_SLEEP_TS;
}
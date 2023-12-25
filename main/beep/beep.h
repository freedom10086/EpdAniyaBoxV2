//
// Created by yang on 2023/12/19.
//

#ifndef HELLO_WORLD_BEEP_H
#define HELLO_WORLD_BEEP_H

#include "esp_types.h"
#include "esp_err.h"

/**
 * level 1 beep
 */
#define BEEP_GPIO_NUM 15

typedef enum {
    BEEP_MODE_PWM,
    BEEP_MODE_RMT
} beep_mode_t;

/**
 * freq = 2 ^ ((p - 69) / 12) * 440
 * p is midi num
 * https://zh.wikipedia.org/wiki/%E9%9F%B3%E7%AC%A6
 */
typedef enum note {
    NOTE_NONE = 0,

    NOTE1_BASS = 262,
    NOTE1_BASS_H = 277, // 1#
    NOTE2_BASS = 294,
    NOTE2_BASS_H = 311,
    NOTE3_BASS = 330,
    NOTE4_BASS = 349,
    NOTE4_BASS_H = 370,
    NOTE5_BASS = 392,
    NOTE5_BASS_H = 415,
    NOTE6_BASS = 440,
    NOTE6_BASS_H = 466,
    NOTE7_BASS = 494,

    NOTE1 = 523,
    NOTE1_H = 554, // 1#
    NOTE2 = 587,
    NOTE2_H = 622,
    NOTE3 = 659,
    NOTE4 = 698,
    NOTE4_H = 740,
    NOTE5 = 784,
    NOTE5_H = 831,
    NOTE6 = 880,
    NOTE6_H = 932,
    NOTE7 = 988,

    NOTE1_HIGH = 1046,
    NOTE1_HIGH_H = 1109, // 1.#
    NOTE2_HIGH = 1175,
    NOTE2_HIGH_H = 1245,
    NOTE3_HIGH = 1318,
    NOTE4_HIGH = 1397,
    NOTE4_HIGH_H = 1480,
    NOTE5_HIGH = 1568,
    NOTE5_HIGH_H = 1661,
    NOTE6_HIGH = 1760,
    NOTE6_HIGH_H = 1865,
    NOTE7_HIGH = 1976,

    NOTE1_HIGH_HIGH = 2093,
    NOTE1_HIGH_HIGH_H = 2217, // 1..#
    NOTE2_HIGH_HIGH = 2349,
    NOTE2_HIGH_HIGH_H = 2489,
    NOTE3_HIGH_HIGH = 2637,
    NOTE4_HIGH_HIGH = 2794,
    NOTE4_HIGH_HIGH_H = 2960,
    NOTE5_HIGH_HIGH = 3136,
    NOTE5_HIGH_HIGH_H = 3322,
    NOTE6_HIGH_HIGH = 3520,
    NOTE6_HIGH_HIGH_H = 3729,
    NOTE7_HIGH_HIGH = 3951,
} note_t;


/**
 * @brief Type of buzzer musical score
 */
typedef struct {
    uint32_t freq_hz;     /*!< Frequency, in Hz if 0 stop beep */
    uint32_t duration_ms; /*!< Duration, in ms */
} buzzer_musical_score_t;

/**
 * @brief Musical Score: Beethoven's Ode to joy
 */
static const buzzer_musical_score_t music_score_1[] = {
        {740, 400},
        {740, 600},
        {784, 400},
        {880, 400},
        {880, 400},
        {784, 400},
        {740, 400},
        {659, 400},
        {587, 400},
        {587, 400},
        {659, 400},
        {740, 400},
        {740, 400},
        {740, 200},
        {659, 200},
        {659, 800},

        {740, 400},
        {740, 600},
        {784, 400},
        {880, 400},
        {880, 400},
        {784, 400},
        {740, 400},
        {659, 400},
        {587, 400},
        {587, 400},
        {659, 400},
        {740, 400},
        {659, 400},
        {659, 200},
        {587, 200},
        {587, 800},

        {659, 400},
        {659, 400},
        {740, 400},
        {587, 400},
        {659, 400},
        {740, 200},
        {784, 200},
        {740, 400},
        {587, 400},
        {659, 400},
        {740, 200},
        {784, 200},
        {740, 400},
        {659, 400},
        {587, 400},
        {659, 400},
        {440, 400},
        {440, 400},

        {740, 400},
        {740, 600},
        {784, 400},
        {880, 400},
        {880, 400},
        {784, 400},
        {740, 400},
        {659, 400},
        {587, 400},
        {587, 400},
        {659, 400},
        {740, 400},
        {659, 400},
        {659, 200},
        {587, 200},
        {587, 800},
};

static const buzzer_musical_score_t normal_beep_score[] = {
        {4000, 600},
};

static const buzzer_musical_score_t normal_beep_beep_score[] = {
        {4000, 500,},
        {0,    300,},
        {4000, 500}
};

#define NOTE_TS_NORMAL 500
static const buzzer_musical_score_t music_score_hszy[] = {
        {NOTE6_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE7_HIGH,      NOTE_TS_NORMAL / 2},

        {NOTE1_HIGH_HIGH, NOTE_TS_NORMAL},
        {NOTE7_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE6_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE3_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE2_HIGH,      NOTE_TS_NORMAL / 2},

        {NOTE3_HIGH,      NOTE_TS_NORMAL},
        {NOTE6_HIGH,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE3_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL / 2},

        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL / 2},
        {NOTE3_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL * 3 / 2}, // NOTE4_HIGH ?
        {NOTE2,           NOTE_TS_NORMAL / 2},

        {NOTE3_HIGH,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE6,           NOTE_TS_NORMAL / 2},
        {NOTE7,           NOTE_TS_NORMAL / 2},

        {NOTE1_HIGH,      NOTE_TS_NORMAL},
        {NOTE7,           NOTE_TS_NORMAL / 2},
        {NOTE6,           NOTE_TS_NORMAL / 2},
        {NOTE5,           NOTE_TS_NORMAL},
        {NOTE2_HIGH,      NOTE_TS_NORMAL},

        {NOTE3_HIGH,      NOTE_TS_NORMAL},
        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE2_HIGH,      NOTE_TS_NORMAL},
        {NOTE1_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE7,           NOTE_TS_NORMAL / 2},

        {NOTE6,           NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE6,           NOTE_TS_NORMAL * 3 / 2},
        {NOTE5,           NOTE_TS_NORMAL / 2},

        {NOTE6,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},

        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},

        {NOTE6_BASS,      NOTE_TS_NORMAL / 2},
        {NOTE3,           NOTE_TS_NORMAL / 2},
        {NOTE2,           NOTE_TS_NORMAL / 2},
        {NOTE3,           NOTE_TS_NORMAL / 2},
        {NOTE5,           NOTE_TS_NORMAL},
        {NOTE3,           NOTE_TS_NORMAL / 2},
        {NOTE2,           NOTE_TS_NORMAL / 2},

        {NOTE3,           NOTE_TS_NORMAL / 2},
        {NOTE5,           NOTE_TS_NORMAL / 2},
        {NOTE2,           NOTE_TS_NORMAL},
        {NOTE1,           NOTE_TS_NORMAL / 2},
        {NOTE6_BASS,      NOTE_TS_NORMAL / 2},
        {NOTE1,           NOTE_TS_NORMAL / 2},
        {NOTE2,           NOTE_TS_NORMAL / 2},

        {NOTE3,           NOTE_TS_NORMAL},
        {NOTE5,           NOTE_TS_NORMAL},
        {NOTE5,           NOTE_TS_NORMAL},
        {NOTE6,           NOTE_TS_NORMAL},

        {NOTE3,           NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE3,           NOTE_TS_NORMAL / 2},
        {NOTE6,           NOTE_TS_NORMAL / 2},

        {NOTE6,           NOTE_TS_NORMAL * 3 / 2},
        {NOTE5,           NOTE_TS_NORMAL / 2},
        {NOTE3,           NOTE_TS_NORMAL / 2},
        {NOTE2,           NOTE_TS_NORMAL / 2},
        {NOTE1,           NOTE_TS_NORMAL / 2},
        {NOTE2,           NOTE_TS_NORMAL / 2},

        {NOTE3,           NOTE_TS_NORMAL},
        {NOTE2,           NOTE_TS_NORMAL / 2},
        {NOTE1,           NOTE_TS_NORMAL / 2},
        {NOTE6_BASS,      NOTE_TS_NORMAL},
        {NOTE3,           NOTE_TS_NORMAL / 2},
        {NOTE2,           NOTE_TS_NORMAL / 2},

        {NOTE6_BASS,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE6_BASS,      NOTE_TS_NORMAL * 3 / 2},
        {NOTE5_BASS,      NOTE_TS_NORMAL / 2},

        {NOTE6_BASS,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE6_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE7_HIGH,      NOTE_TS_NORMAL / 2},

        {NOTE1_HIGH_HIGH, NOTE_TS_NORMAL},
        {NOTE7_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE6_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE3_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE2_HIGH,      NOTE_TS_NORMAL / 2},

        {NOTE3_HIGH,      NOTE_TS_NORMAL},
        {NOTE6_HIGH,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE3_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL / 2},

        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL / 2},
        {NOTE3_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL * 3 / 2}, // NOTE4_HIGH ?
        {NOTE2_HIGH,         NOTE_TS_NORMAL / 2},

        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL / 2},
        {NOTE3_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE4_HIGH_H,    NOTE_TS_NORMAL * 3 / 2},
        {NOTE2_H,         NOTE_TS_NORMAL / 2},

        {NOTE3_HIGH,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE6_HIGH,      NOTE_TS_NORMAL / 2},
        {NOTE7_HIGH,      NOTE_TS_NORMAL / 2},

        {NOTE1_HIGH_HIGH,      NOTE_TS_NORMAL},
        {NOTE7_HIGH,       NOTE_TS_NORMAL/2},
        {NOTE6_HIGH,       NOTE_TS_NORMAL/2},
        {NOTE5_HIGH,      NOTE_TS_NORMAL},
        {NOTE2_HIGH_HIGH,      NOTE_TS_NORMAL},

        {NOTE3_HIGH_HIGH,      NOTE_TS_NORMAL},
        {NOTE5_HIGH_HIGH,       NOTE_TS_NORMAL},
        {NOTE2_HIGH_HIGH,       NOTE_TS_NORMAL},
        {NOTE1_HIGH_HIGH,      NOTE_TS_NORMAL/2},
        {NOTE7_HIGH,      NOTE_TS_NORMAL/2},

        {NOTE6_HIGH,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE6_HIGH,       NOTE_TS_NORMAL * 3 / 2},
        {NOTE5_HIGH,      NOTE_TS_NORMAL/2},

        {NOTE6_HIGH,      NOTE_TS_NORMAL / 4},
        {NOTE5_HIGH,      NOTE_TS_NORMAL / 4},
        {NOTE6_HIGH,       NOTE_TS_NORMAL / 2},
        {NOTE6_HIGH,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,      NOTE_TS_NORMAL},

        {NOTE5_HIGH_HIGH,      NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,       NOTE_TS_NORMAL},
        {NOTE_NONE,      NOTE_TS_NORMAL},
};

static const buzzer_musical_score_t music_score_xxx[] = {
        {NOTE1, NOTE_TS_NORMAL},
        {NOTE1, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL},

        {NOTE6, NOTE_TS_NORMAL},
        {NOTE6, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL * 2},

        {NOTE4, NOTE_TS_NORMAL},
        {NOTE4, NOTE_TS_NORMAL},
        {NOTE3, NOTE_TS_NORMAL},
        {NOTE3, NOTE_TS_NORMAL},

        {NOTE2, NOTE_TS_NORMAL},
        {NOTE2, NOTE_TS_NORMAL},
        {NOTE1, NOTE_TS_NORMAL * 2},

        {NOTE5, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL},
        {NOTE4, NOTE_TS_NORMAL},
        {NOTE4, NOTE_TS_NORMAL},

        {NOTE3, NOTE_TS_NORMAL},
        {NOTE3, NOTE_TS_NORMAL},
        {NOTE2, NOTE_TS_NORMAL * 2},

        {NOTE5, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL},
        {NOTE4, NOTE_TS_NORMAL},
        {NOTE4, NOTE_TS_NORMAL},

        {NOTE3, NOTE_TS_NORMAL},
        {NOTE3, NOTE_TS_NORMAL},
        {NOTE2, NOTE_TS_NORMAL * 2},

        {NOTE1, NOTE_TS_NORMAL},
        {NOTE1, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL},

        {NOTE6, NOTE_TS_NORMAL},
        {NOTE6, NOTE_TS_NORMAL},
        {NOTE5, NOTE_TS_NORMAL * 2},

        {NOTE4, NOTE_TS_NORMAL},
        {NOTE4, NOTE_TS_NORMAL},
        {NOTE3, NOTE_TS_NORMAL},
        {NOTE3, NOTE_TS_NORMAL},

        {NOTE2, NOTE_TS_NORMAL},
        {NOTE2, NOTE_TS_NORMAL},
        {NOTE1, NOTE_TS_NORMAL * 2},
};

esp_err_t beep_init(beep_mode_t mode);

/**
 *  duration ms if = 0 no stop beep
 */
esp_err_t beep_start_beep(uint32_t duration);

esp_err_t beep_start_play(const buzzer_musical_score_t *song, uint16_t song_len);

esp_err_t stop_beep();

esp_err_t beep_deinit();

#endif //HELLO_WORLD_BEEP_H

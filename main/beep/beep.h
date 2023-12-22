//
// Created by yang on 2023/12/19.
//

#ifndef HELLO_WORLD_BEEP_H
#define HELLO_WORLD_BEEP_H

#include "esp_types.h"
#include "esp_err.h"

#define BEEP_GPIO_NUM 15

typedef enum {
    PWM,
    RMT
} beep_mode_t;

/**
 * @brief Type of buzzer musical score
 */
typedef struct {
    uint32_t freq_hz;     /*!< Frequency, in Hz */
    uint32_t duration_ms; /*!< Duration, in ms */
} buzzer_musical_score_t;

/**
 * @brief Musical Score: Beethoven's Ode to joy
 */
static const buzzer_musical_score_t music_score_1[] = {
        {740, 400}, {740, 600}, {784, 400}, {880, 400},
        {880, 400}, {784, 400}, {740, 400}, {659, 400},
        {587, 400}, {587, 400}, {659, 400}, {740, 400},
        {740, 400}, {740, 200}, {659, 200}, {659, 800},

        {740, 400}, {740, 600}, {784, 400}, {880, 400},
        {880, 400}, {784, 400}, {740, 400}, {659, 400},
        {587, 400}, {587, 400}, {659, 400}, {740, 400},
        {659, 400}, {659, 200}, {587, 200}, {587, 800},

        {659, 400}, {659, 400}, {740, 400}, {587, 400},
        {659, 400}, {740, 200}, {784, 200}, {740, 400}, {587, 400},
        {659, 400}, {740, 200}, {784, 200}, {740, 400}, {659, 400},
        {587, 400}, {659, 400}, {440, 400}, {440, 400},

        {740, 400}, {740, 600}, {784, 400}, {880, 400},
        {880, 400}, {784, 400}, {740, 400}, {659, 400},
        {587, 400}, {587, 400}, {659, 400}, {740, 400},
        {659, 400}, {659, 200}, {587, 200}, {587, 800},
};

static const buzzer_musical_score_t normal_beep_score[] = {
        {4000, 1000},
};

esp_err_t beep_init(beep_mode_t mode);

/**
 *  duration ms if = 0 no stop beep
 */
esp_err_t start_beep(uint32_t duration);

esp_err_t stop_beep();

#endif //HELLO_WORLD_BEEP_H

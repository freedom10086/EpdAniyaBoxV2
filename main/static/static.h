#ifndef STATIC_H
#define STATIC_H

extern const uint8_t aniya_200_1_bmp_start[] asm("_binary_aniya_200_1_bmp_start");
extern const uint8_t aniya_200_1_bmp_end[] asm("_binary_aniya_200_1_bmp_end");

extern const uint8_t icon_ble_bmp_start[] asm("_binary_icon_ble_bmp_start");
extern const uint8_t icon_ble_bmp_end[] asm("_binary_icon_ble_bmp_end");

extern const uint8_t icon_sat_bmp_start[] asm("_binary_icon_sat_bmp_start");
extern const uint8_t icon_sat_bmp_end[] asm("_binary_icon_sat_bmp_end");

extern const uint8_t icon_upgrade_bmp_start[] asm("_binary_icon_upgrade_bmp_start");
extern const uint8_t icon_upgrade_bmp_end[] asm("_binary_icon_upgrade_bmp_end");

extern const uint8_t ic_home_bmp_start[] asm("_binary_ic_home_32_bmp_start");
extern const uint8_t ic_home_bmp_end[] asm("_binary_ic_home_32_bmp_end");

extern const uint8_t ic_image_bmp_start[] asm("_binary_ic_image_32_bmp_start");
extern const uint8_t ic_image_bmp_end[] asm("_binary_ic_image_32_bmp_end");

extern const uint8_t ic_manual_bmp_start[] asm("_binary_ic_manual_32_bmp_start");
extern const uint8_t ic_manual_bmp_end[] asm("_binary_ic_manual_32_bmp_end");

extern const uint8_t ic_setting_bmp_start[] asm("_binary_ic_setting_32_bmp_start");
extern const uint8_t ic_setting_bmp_end[] asm("_binary_ic_setting_32_bmp_end");

extern const uint8_t ic_info_bmp_start[] asm("_binary_ic_info_32_bmp_start");
extern const uint8_t ic_info_bmp_end[] asm("_binary_ic_info_32_bmp_end");

extern const uint8_t ic_upgrade_bmp_start[] asm("_binary_ic_upgrade_32_bmp_start");
extern const uint8_t ic_upgrade_bmp_end[] asm("_binary_ic_upgrade_32_bmp_end");

extern const uint8_t ic_close_bmp_start[] asm("_binary_ic_close_32_bmp_start");
extern const uint8_t ic_close_bmp_end[] asm("_binary_ic_close_32_bmp_end");

extern const uint8_t ic_reboot_bmp_start[] asm("_binary_ic_reboot_32_bmp_start");
extern const uint8_t ic_reboot_bmp_end[] asm("_binary_ic_reboot_32_bmp_end");

extern const uint8_t ic_back_bmp_start[] asm("_binary_ic_back_32_bmp_start");
extern const uint8_t ic_back_bmp_end[] asm("_binary_ic_back_32_bmp_end");

extern const uint8_t ic_ble_bmp_start[] asm("_binary_ic_ble_32_bmp_start");
extern const uint8_t ic_ble_bmp_end[] asm("_binary_ic_ble_32_bmp_end");

extern const uint8_t ic_music_bmp_start[] asm("_binary_ic_music_32_bmp_start");
extern const uint8_t ic_music_bmp_end[] asm("_binary_ic_music_32_bmp_end");

extern const uint8_t ic_battery_bmp_start[] asm("_binary_ic_battery_32_bmp_start");
extern const uint8_t ic_battery_bmp_end[] asm("_binary_ic_battery_32_bmp_end");

extern const uint8_t ic_tomato_bmp_start[] asm("_binary_ic_tomato_32_bmp_start");
extern const uint8_t ic_tomato_bmp_end[] asm("_binary_ic_tomato_32_bmp_end");

extern const uint8_t ic_studying_bmp_start[] asm("_binary_ic_studying_32_bmp_start");
extern const uint8_t ic_studying_bmp_end[] asm("_binary_ic_studying_32_bmp_end");

extern const uint8_t ic_playing_bmp_start[] asm("_binary_ic_playing_32_bmp_start");
extern const uint8_t ic_playing_bmp_end[] asm("_binary_ic_playing_32_bmp_end");

extern const uint8_t ic_summary_bmp_start[] asm("_binary_ic_summary_32_bmp_start");
extern const uint8_t ic_summary_bmp_end[] asm("_binary_ic_summary_32_bmp_end");

// https://www.qqxiuzi.cn/bianma/zifuji.php
// 蓝牙已打开
static const uint8_t text_ble_on[] = {0xC0, 0xB6, 0xD1, 0xC0, 0xD2, 0xD1, 0xB4, 0xF2, 0xBF, 0xAA, 0x00};
// 时间未校准
static const uint8_t text_datetime_need_set[] = {0xCA, 0xB1, 0xBC, 0xE4, 0xCE, 0xB4, 0xD0, 0xA3, 0xD7, 0xBC, 0x00};

// 开始
static const uint8_t text_start[] = {0xBF, 0xAA, 0xCA, 0xBC, 0x00};

// 学习时间（分钟）
static const uint8_t text_study_time[] = {0xD1, 0xA7, 0xCF, 0xB0, 0xCA, 0xB1, 0xBC, 0xE4, 0xA3, 0xA8, 0xB7, 0xD6, 0xD6,
                                          0xD3, 0xA3, 0xA9, 0x00};

// 休息时间（分钟）
static const uint8_t text_play_time[] = {0xD0, 0xDD, 0xCF, 0xA2, 0xCA, 0xB1, 0xBC, 0xE4, 0xA3, 0xA8, 0xB7, 0xD6, 0xD6,
                                         0xD3, 0xA3, 0xA9, 0x00};

// 循环次数
static const uint8_t text_loop_count[] = {0xD1, 0xAD, 0xBB, 0xB7, 0xB4, 0xCE, 0xCA, 0xFD, 0x00};

// 学习中～
static const uint8_t text_studying[] = {0xD1, 0xA7, 0xCF, 0xB0, 0xD6, 0xD0, 0xA1, 0xAB, 0x00};

// 休息中～
static const uint8_t text_playing[] = {0xD0, 0xDD, 0xCF, 0xA2, 0xD6, 0xD0, 0xA1, 0xAB, 0x00};

// 总结
static const uint8_t text_summary[] = {0xD7, 0xDC, 0xBD, 0xE1, 0x00};

// 番茄时钟
static const uint8_t text_tomato_clock[] = {0xB7, 0xAC, 0xC7, 0xD1, 0xCA, 0xB1, 0xD6, 0xD3, 0x00};

// 闹钟
static const uint8_t text_alarm_clock[] = {0xC4, 0xD6, 0xD6, 0xD3, 0x00};

// 重复
static const uint8_t text_repeat[] = {0xD6, 0xD8, 0xB8, 0xB4, 0x00};

// 星期
static const uint16_t text_week_label[] = {0xC7D0, 0xDAC6, 0x00};

static const uint16_t text_week_num[][2] = {
        {0xD5C8, 0x00}, // 日
        {0xBBD2, 0x00}, // 一
        {0xFEB6, 0x00}, // 二
        {0xFDC8, 0x00}, // 三
        {0xC4CB, 0x00}, // 四
        {0xE5CE, 0x00}, // 五
        {0xF9C1, 0x00}, // 六
};

// 保存
static const uint8_t text_save[] = {0xB1, 0xA3, 0xB4, 0xE6, 0x00};

#endif
file(GLOB VIEW_SOURCE_FILES
        "lcd/*.c"
        "view/*.c"
        "page/*.c"
)

file(GLOB BLE_SOURCE_FILES
        "ble/*.c"
        "bles/*.c"
)

set(srcs "tools/kalman_filter.c" "tools/encode.c"
        "battery.c" "key.c" "setting.c"
        "file/my_file_common.c"
        "common_utils.c"
        "sht40.c" "LIS3DH.c" "max31328.c" "sgp30.c" "spl06.c" "opt3001.c"
        "beep/beep.c" "beep/musical_score_encoder.c" "page_manager.c"
        "main.c")

list(APPEND srcs ${VIEW_SOURCE_FILES} ${BLE_SOURCE_FILES})

idf_component_register(SRCS ${srcs}
        EMBED_FILES "lcd/HZK16.bin"
        EMBED_FILES "static/aniya_200_1.bmp"
        EMBED_FILES "static/icon_ble.bmp" "static/icon_sat.bmp" "static/icon_upgrade.bmp"
        EMBED_FILES "static/ic_close_32.bmp" "static/ic_home_32.bmp" "static/ic_image_32.bmp" "static/ic_info_32.bmp" "static/ic_manual_32.bmp"
        EMBED_FILES "static/ic_setting_32.bmp" "static/ic_upgrade_32.bmp" "static/ic_reboot_32.bmp" "static/ic_back_32.bmp" "static/ic_ble_32.bmp"
        EMBED_FILES "static/ic_music_32.bmp" "static/ic_battery_32.bmp" "static/ic_tomato_32.bmp" "static/ic_studying_32.bmp" "static/ic_playing_32.bmp"
        EMBED_FILES "static/ic_summary_32.bmp" "static/ic_alarm_32.bmp"
        INCLUDE_DIRS ".")
//
// Created by yang on 2024/4/7.
//

#ifndef ANIYA_BOX_V2_PRESSURE_SENSOR_PAGE_H
#define ANIYA_BOX_V2_PRESSURE_SENSOR_PAGE_H


void pressure_sensor_page_on_create(void *arg);

void pressure_sensor_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt);

bool pressure_sensor_page_key_click(key_event_id_t key_event_type);

int pressure_sensor_page_on_enter_sleep(void *args);

void pressure_sensor_page_on_destroy(void *arg);

#endif //ANIYA_BOX_V2_PRESSURE_SENSOR_PAGE_H

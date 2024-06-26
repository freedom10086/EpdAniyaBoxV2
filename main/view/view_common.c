//
// Created by yang on 2024/3/17.
//

#include "view_common.h"

void view_set_click_cb(view_t *view, view_on_click_cb cb) {
    view->view_on_click_cb = cb;
}

void view_set_value_change_cb(view_t *view, view_on_value_change_cb cb) {
    view->view_on_value_change_cb = cb;
}
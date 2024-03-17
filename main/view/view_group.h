//
// Created by yang on 2024/3/16.
//

#ifndef ANIYA_BOX_V2_VIEW_GROUP_H
#define ANIYA_BOX_V2_VIEW_GROUP_H

#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"

#include "lcd/epdpaint.h"

#include "view_common.h"

struct view_element_t {
    view_t *v;
    struct view_element_t *next;
} ;

typedef struct {
    int current_select_index;
    int element_count;
    struct view_element_t *head;
} view_group_view_t;

view_group_view_t *view_group_create();

void view_group_add_view(view_group_view_t *group, view_t *v);

void view_group_remove_view(view_group_view_t *group, view_t *v);

uint8_t view_group_view_count(view_group_view_t *group);

view_t *view_group_get_view(view_group_view_t *group, uint8_t index);

void view_group_delete(view_group_view_t * v);

struct view_element_t *view_group_get_current_focus(view_group_view_t * v);

bool view_group_focus_pre(view_group_view_t * v);

bool view_group_focus_next(view_group_view_t * v);

void view_group_select_current();

bool view_group_handle_key_event(view_group_view_t *group, key_event_id_t event);

#endif //ANIYA_BOX_V2_VIEW_GROUP_H

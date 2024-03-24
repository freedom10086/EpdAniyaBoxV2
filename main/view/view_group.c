//
// Created by yang on 2024/3/16.
//

#include "view_group.h"
#include "page_manager.h"

#include "esp_log.h"

static const char* TAG = "view_group";

view_group_view_t *view_group_create() {
    view_group_view_t *group = malloc(sizeof(view_group_view_t));
    group->element_count = 0;
    group->current_select_index = -1;
    group->head = NULL;
    return group;
}

void view_group_add_view(view_group_view_t *group, view_t *v) {
    struct view_element_t *head = group->head;
    while (head != NULL && head->next != NULL) {
        head = head->next;
    }

    struct view_element_t *ele = malloc(sizeof(struct view_element_t));
    ele->v = v;
    ele->next = NULL;

    if (head == NULL) {
        // first element
        group->head = ele;
    } else { // head -> next is null
        head->next = ele;
    }

    group->element_count += 1;
}

void view_group_remove_view(view_group_view_t *group, view_t *v) {
    struct view_element_t *pre = NULL;
    struct view_element_t *item = group->head;
    while (item != NULL) {
        if (item->v == v) {
            break;
        }
        pre = item;
        item = item->next;
    }

    if (item != NULL && item->v == v) {
        if (pre == NULL) {
            group->head = item->next;
        } else {
            pre->next = item->next;
        }

        group->element_count -= 1;

        item->next = NULL;
        free(item);
    }
}

uint8_t view_group_view_count(view_group_view_t *group) {
    return group->element_count;
}

view_t *view_group_get_view(view_group_view_t *group, uint8_t index) {
    assert(index < group->element_count);
    struct view_element_t *item = group->head;
    for (; index > 0; index--) {
        item = item->next;
    }
    return item->v;
}

static struct view_element_t *find_pre_view(view_group_view_t *group, struct view_element_t* curr) {
    struct view_element_t *pre = NULL;
    struct view_element_t *item = group->head;
    while (item != NULL) {
        if (item == curr) {
            break;
        }
        pre = item;
        item = item->next;
    }
    return pre;
}

static struct view_element_t *find_last_view(view_group_view_t *group) {
    struct view_element_t *item = group->head;
    while (item != NULL) {
        if (item->next == NULL) {
            break;
        }
        item = item->next;
    }
    return item;
}

bool view_group_focus_pre(view_group_view_t *group) {
    // find current focus and foucs next
    struct view_element_t *curr_focus = view_group_get_current_focus(group);
    if (curr_focus != NULL && curr_focus->v->state == VIEW_STATE_SELECTED) {
        // if current has view is selected cant focus next
        return false;
    }

    struct view_element_t *item;
    if (curr_focus == NULL) {
        // current has no focus, start from last
        item = find_last_view(group);
        while (item != NULL) {
            if (item->v->state != VIEW_STATE_DISABLE) {
                item->v->state = VIEW_STATE_FOCUS;
                return true;
            }
            item = find_pre_view(group, item);
        }
    } else {
        // un focus current
        curr_focus->v->state = VIEW_STATE_NORMAL;

        // find pre view
        item = find_pre_view(group, curr_focus);
        while (item != NULL) {
            if (item->v->state != VIEW_STATE_DISABLE || item == curr_focus) {
                item->v->state = VIEW_STATE_FOCUS;
                return true;
            }

            item = find_pre_view(group, curr_focus);
        }

        // cant find satisfy re find from tail
        item = find_last_view(group);
        while (item != NULL) {
            if (item->v->state != VIEW_STATE_DISABLE || item == curr_focus) {
                item->v->state = VIEW_STATE_FOCUS;
                return true;
            }
        }
    }

    return false;
}

bool view_group_focus_next(view_group_view_t *group) {
    // find current focus and foucs next
    struct view_element_t *curr_focus = view_group_get_current_focus(group);
    if (curr_focus != NULL && curr_focus->v->state == VIEW_STATE_SELECTED) {
        // if current has view is selected cant focus next
        return false;
    }

    if (curr_focus == NULL) {
        // current has no focus, focus first
        if (group->head != NULL && group->head->v->state != VIEW_STATE_DISABLE) {
            group->head->v->state = VIEW_STATE_FOCUS;
            return true;
        }
    } else {
        // un focus current
        curr_focus->v->state = VIEW_STATE_NORMAL;

        struct view_element_t *item = curr_focus->next;
        while (item != NULL) {
            if (item->v->state != VIEW_STATE_DISABLE) {
                item->v->state = VIEW_STATE_FOCUS;
                return true;
            }
            item = item->next;
        }

        // cant find satisfy re find from head
        item = group->head;
        while (item != NULL) {
            if (item->v->state != VIEW_STATE_DISABLE || item == curr_focus) {
                item->v->state = VIEW_STATE_FOCUS;
                return true;
            }
        }
    }

    return false;
}

struct view_element_t *view_group_get_current_focus(view_group_view_t *group) {
    struct view_element_t *item = group->head;
    while (item != NULL) {
        if (item->v->state == VIEW_STATE_FOCUS || item->v->state == VIEW_STATE_SELECTED) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

void view_group_delete(view_group_view_t *v) {
    struct view_element_t *head = v->head;
    struct view_element_t *ele = NULL;

    while (head != NULL) {
        ele = head;
        head = head->next;

        free(ele);
    }

    free(v);
}

bool view_group_handle_key_event(view_group_view_t *group, key_event_id_t event) {
    struct view_element_t *curr_focus = view_group_get_current_focus(group);
    switch (event) {
        case KEY_OK_SHORT_CLICK:
            if (curr_focus != NULL) {
                if (curr_focus->v->state == VIEW_STATE_FOCUS
                    || curr_focus->v->state == VIEW_STATE_SELECTED) {

                    if (curr_focus->v->state == VIEW_STATE_FOCUS && curr_focus->v->selectable) {
                        // select view
                        curr_focus->v->state = VIEW_STATE_SELECTED;
                    }

                    if (curr_focus->v->key_event != NULL) {
                        // pass click event
                        curr_focus->v->key_event(curr_focus->v, event);
                    }

                    page_manager_request_update(false);
                    return true;
                }
            }
            break;
        case KEY_CANCEL_SHORT_CLICK:
            if (curr_focus != NULL) {
                if (curr_focus->v->state == VIEW_STATE_SELECTED) {
                    // unselect view
                    curr_focus->v->state = VIEW_STATE_FOCUS;
                    page_manager_request_update(false);
                    return true;
                }
            }
            break;
        case KEY_UP_SHORT_CLICK:
            if (view_group_focus_pre(group)) {
                page_manager_request_update(false);
                return true;
            }

            if (curr_focus != NULL && curr_focus->v->state == VIEW_STATE_SELECTED) {
                if (curr_focus->v->key_event != NULL) {
                    bool handle = curr_focus->v->key_event(curr_focus->v, event);
                    if (handle) {
                        page_manager_request_update(false);
                    }
                    return handle;
                }
            }
            break;
        case KEY_DOWN_SHORT_CLICK:
            if (view_group_focus_next(group)) {
                page_manager_request_update(false);
                return true;
            }

            if (curr_focus != NULL && curr_focus->v->state == VIEW_STATE_SELECTED) {
                if (curr_focus->v->key_event != NULL) {
                    bool handle = curr_focus->v->key_event(curr_focus->v, event);
                    if (handle) {
                        page_manager_request_update(false);
                    }
                    return handle;
                }
            }
            break;
        default:
            break;
    }
    return false;
}
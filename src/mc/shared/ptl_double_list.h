#ifndef MC_SHARED_PTL_DOUBLE_LIST_H
#define MC_SHARED_PTL_DOUBLE_LIST_H

#include <pthread.h>

struct ptl_double_list_item_t {
    struct ptl_double_list_item_t *next;
    struct ptl_double_list_item_t *prev;
};
typedef struct ptl_double_list_item_t ptl_double_list_item_t;

struct ptl_double_list_t {
    struct ptl_double_list_item_t *head;
    struct ptl_double_list_item_t *tail;
    int want_lock;
    pthread_mutex_t mutex;
};
typedef struct ptl_double_list_t ptl_double_list_t;


int ptl_double_list_init(ptl_double_list_t *list, int want_lock);
int ptl_double_list_fini(ptl_double_list_t *list);


static inline ptl_double_list_item_t*
ptl_double_list_remove_front(ptl_double_list_t *list)
{
    ptl_double_list_item_t *item;

    if (list->want_lock) {
        pthread_mutex_lock(&list->mutex);
    }

    item = list->head;
    if (NULL != item) {
        list->head = item->next;
        if (NULL != item->next) {
            item->next->prev = NULL;
        } else {
            list->tail = NULL;
        }
    }

    if (list->want_lock) {
        pthread_mutex_unlock(&list->mutex);
    }

    return item;
}


static inline void
ptl_double_list_insert_back(ptl_double_list_t *list, ptl_double_list_item_t *item)
{
    if (list->want_lock) {
        pthread_mutex_lock(&list->mutex);
    }

    item->next = NULL;
    item->prev = list->tail;
    list->tail = item;
    if (NULL != item->prev) {
        item->prev->next = item;
    } else {
        list->head = item;
    }

    if (list->want_lock) {
        pthread_mutex_unlock(&list->mutex);
    }
}


static inline void
ptl_double_list_remove_item(ptl_double_list_t *list, ptl_double_list_item_t *item)
{
    if (list->want_lock) {
        pthread_mutex_lock(&list->mutex);
    }

    if (NULL == item->prev) {
        list->head = item->next;
    } else {
        item->prev->next = item->next;
    }
    if (NULL == item->next) {
        list->tail = item->prev;
    } else {
        item->next->prev = item->prev;
    }

    if (list->want_lock) {
        pthread_mutex_unlock(&list->mutex);
    }
}

#endif

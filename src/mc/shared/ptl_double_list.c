#include "config.h"

#include "shared/ptl_double_list.h"


int
ptl_double_list_init(ptl_double_list_t *list, int want_lock)
{
    list->head = list->tail = NULL;
    list->want_lock = want_lock;

    if (list->want_lock) {
        pthread_mutex_init(&list->mutex, NULL);
    }

    return 0;
}


int
ptl_double_list_fini(ptl_double_list_t *list)
{
    if (list->want_lock) {
        pthread_mutex_destroy(&list->mutex);
    }

    return 0;
}

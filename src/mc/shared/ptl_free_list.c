#include "config.h"

#include "shared/ptl_free_list.h"

int
ptl_free_list_init(ptl_free_list_t *free_list, size_t item_size)
{
    free_list->item_size = item_size;
    return ptl_stack_init(&free_list->freeq);
}


int
ptl_free_list_fini(ptl_free_list_t *free_list)
{
    void *buf;

    while (NULL != (buf = ptl_stack_pop(&free_list->freeq))) {
        free(buf);
    }

    return ptl_stack_fini(&free_list->freeq);
}

#ifndef MC_SHARED_PTL_FREE_LIST_H
#define MC_SHARED_PTL_FREE_LIST_H

#include "shared/ptl_stack.h"


struct ptl_free_list_t {
    ptl_stack_t freeq;
    size_t item_size;
};
typedef struct ptl_free_list_t ptl_free_list_t;


int ptl_free_list_init(ptl_free_list_t *free_list, size_t item_size);
int ptl_free_list_fini(ptl_free_list_t *free_list);


static inline void*
ptl_free_list_alloc(ptl_free_list_t *free_list)
{
    void *buf;

    buf = ptl_stack_pop(&free_list->freeq);
    if (NULL == buf) {
        buf = malloc(free_list->item_size);
    }

    return buf;
}


static inline void
ptl_free_list_free(ptl_free_list_t *free_list, void *buf)
{
    ptl_stack_push(&free_list->freeq, buf);
}

#endif

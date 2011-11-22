#ifndef MC_SHARED_PTL_STACK_H
#define MC_SHARED_PTL_STACK_H

#include <stdlib.h>

struct ptl_stack_item_t {
    struct ptl_stack_item_t *next;
    void *ptr;
};
typedef struct ptl_stack_item_t ptl_stack_item_t;

struct ptl_stack_t {
    struct ptl_stack_item_t *head;
    struct ptl_stack_item_t *freeq;
};
typedef struct ptl_stack_t ptl_stack_t;


int ptl_stack_init(ptl_stack_t *stack);
int ptl_stack_fini(ptl_stack_t *stack);


static inline void*
ptl_stack_pop(ptl_stack_t *stack)
{
    ptl_stack_item_t *item = NULL;
    void *ret;

    /* get item structure off list */
    do {
        item = stack->head;
        if (NULL == item) return NULL;
    } while (!__sync_bool_compare_and_swap(&stack->head,
                                           item,
                                           item->next));

    /* get pointer */
    ret = item->ptr;

    /* save item structure to freeq */
    do {
        item->next = stack->freeq;
    } while (!__sync_bool_compare_and_swap(&stack->freeq,
                                           item,
                                           item->next));
    return ret;
}

static inline void
ptl_stack_push(ptl_stack_t *stack, void *val)
{
    ptl_stack_item_t *item = NULL;

    /* get an item structure off the freeq or allocate one */
    do {
        item = stack->freeq;
        if (NULL == item) break;
    } while (!__sync_bool_compare_and_swap(&stack->freeq,
                                           item,
                                           item->next));
    if (NULL == item) {
        item = malloc(sizeof(ptl_stack_item_t));
    }

    /* save pointer */
    item->ptr = val;

    /* add item structure to list */
    do {
        item->next = stack->head;
    } while (!__sync_bool_compare_and_swap(&stack->head,
                                           item->next,
                                           item));
}

#endif

#include "config.h"

#include "shared/ptl_stack.h"

int
ptl_stack_init(ptl_stack_t *stack)
{
    stack->head = NULL;
    stack->freeq = NULL;

    return 0;
}


int
ptl_stack_fini(ptl_stack_t *stack)
{
    ptl_stack_item_t *item;

    while (NULL != (item = stack->head)) {
        stack->head = item->next;
        free(item);
    }

    while (NULL != (item = stack->freeq)) {
        stack->freeq = item->next;
        free(item);
    }

    return 0;
}

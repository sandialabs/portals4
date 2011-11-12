#include "config.h"

#include <stdlib.h>

#include "shared/ptl_util.h"
#include "shared/ptl_circular_buffer.h"

int
ptl_circular_buffer_init(ptl_circular_buffer_t **cb,
                         int num_entries, int entry_size)
{
    ptl_circular_buffer_t *tmp;

    num_entries = ptl_find_higher_pow2(num_entries);

    tmp = malloc(sizeof(ptl_circular_buffer_t) +
                 num_entries * entry_size);
    if (NULL == tmp) return -1;

    tmp->head = tmp->tail = tmp->cursor = 0;
    tmp->mask = num_entries - 1;
    tmp->entry_size = num_entries;

    return 0;
}


int
ptl_circular_buffer_fini(ptl_circular_buffer_t *cb)
{
    free(cb);
    return 0;
}

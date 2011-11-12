#ifndef PTL_CIRCULAR_BUFFER_T
#define PTL_CIRCULAR_BUFFER_T

#include <stdint.h>
#include "ptl_internal_alignment.h"

struct ptl_circular_buffer_t {
    uint64_t head;
    uint8_t pad1[CACHELINE_WIDTH - sizeof(uint64_t)];
    uint64_t tail;
    uint64_t cursor;
    uint8_t pad2[CACHELINE_WIDTH - (2 * sizeof(uint64_t))];
    uint64_t mask;
    uint32_t entry_size;
    uint32_t num_entries;
    unsigned char data[];
};
typedef struct ptl_circular_buffer_t ptl_circular_buffer_t ALIGNED (CACHELINE_WIDTH);

int ptl_circular_buffer_init(ptl_circular_buffer_t **cb,
                             int num_entries, int entry_size);

int ptl_circular_buffer_fini(ptl_circular_buffer_t *cb);


static inline int
ptl_circular_buffer_add_overwrite(ptl_circular_buffer_t *cb,
                                  void *entry)
{
    uint64_t tmp;

    tmp = __sync_fetch_and_add(&cb->head, 1);
    tmp &= cb->mask;
    tmp *= cb->entry_size;
    memcpy(&cb->data[tmp], entry, cb->entry_size);

    while (!__sync_bool_compare_and_swap(&cb->cursor,
                                         tmp,
                                         tmp + 1)) {}
    return 0;
}


static inline int
ptl_circular_buffer_add(ptl_circular_buffer_t *cb,
                        void *entry)
{
    uint64_t tmp;

    __sync_synchronize();
    do  {
        tmp = cb->head;
        if (tmp >= cb->tail + cb->num_entries) return 1;
    } while (!__sync_bool_compare_and_swap(&cb->head,
                                           tmp,
                                           tmp + 1));

    tmp &= cb->mask;
    tmp *= cb->entry_size;
    memcpy(&cb->data[tmp], entry, cb->entry_size);

    while (!__sync_bool_compare_and_swap(&cb->cursor,
                                         tmp,
                                         tmp + 1)) {}
    return 0;
}


static inline int
ptl_circular_buffer_get(ptl_circular_buffer_t *cb, 
                        void *entry, int *overwrite)
{
    uint64_t tmp;

    __sync_synchronize();
    do {
        tmp = cb->tail;
        if (tmp == cb->cursor) return 1;
    } while (!__sync_bool_compare_and_swap(&cb->tail,
                                           tmp,
                                           tmp + 1));

    *overwrite = (tmp <= cb->tail + cb->num_entries);
    tmp &= cb->mask;
    tmp *= cb->entry_size;
    memcpy(entry, &cb->data[tmp], cb->entry_size);

    return 0;
}

#endif

#ifndef MC_LIB_ALLOC_H
#define MC_LIB_ALLOC_H

#include <stddef.h>

int alloc_init(void);

int alloc_fini(void);

void alloc_add_mem(void *start, size_t length);

void alloc_remove_mem(void *start, size_t length);

#endif

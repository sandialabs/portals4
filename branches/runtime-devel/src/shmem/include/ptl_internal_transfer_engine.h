#ifndef PTL_INTERNAL_TRANSFER_ENGINE_H
#define PTL_INTERNAL_TRANSFER_ENGINE_H

/* The API definition */
#include <portals4.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_knem.h"
//#include "ptl_internal_xpmem.h"

/* XFE function declarations */
void     INTERNAL   xfe_init(void);
uint64_t INTERNAL   xfe_register_singleuse(void *, ptl_size_t, int);
uint64_t INTERNAL   xfe_register(void *, ptl_size_t, int);
void     INTERNAL   xfe_unregister(uint64_t);
size_t   INTERNAL   xfe_copy_from(void *, uint64_t, uint64_t, size_t);
size_t   INTERNAL   xfe_copy_to(uint64_t, uint64_t, void *, size_t);
void     INTERNAL * xfe_attach(uint64_t);

#endif /* ifndef PTL_INTERNAL_TRANSFER_ENGINE_H */
/* vim:set expandtab: */

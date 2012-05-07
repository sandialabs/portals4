#ifndef PTL_INTERNAL_KNEM_H
#define PTL_INTERNAL_KNEM_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* The API definition */
#include <portals4.h>

/* System headers */
#include <stddef.h>                    /* for size_t */

/* Internals */
#include "ptl_visibility.h"

#ifdef USE_KNEM
# include <knem_io.h>
/* KNEM XFE function declarations */
void     INTERNAL   knem_init(void);
uint64_t INTERNAL   knem_register_singleuse(void *, ptl_size_t, int);
uint64_t INTERNAL   knem_register(void *, ptl_size_t, int);
void     INTERNAL   knem_unregister(uint64_t);
size_t   INTERNAL   knem_copy_from(void *, uint64_t, uint64_t, size_t);
size_t   INTERNAL   knem_copy_to(uint64_t, uint64_t, void *, size_t);
void     INTERNAL * knem_attach(uint64_t);
#endif /* ifdef USE_KNEM */

#endif /* ifndef PTL_INTERNAL_KNEM_H */
/* vim:set expandtab: */

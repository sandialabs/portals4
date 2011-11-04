#ifndef PTL_INTERNAL_NIT_H
#define PTL_INTERNAL_NIT_H

#include <stdint.h>                    /* for uint32_t */

#include "ptl_visibility.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_PT.h"

typedef struct {
    ptl_table_entry_t              *tables[4];
    uint32_t                        refcount[4];
#if 0
    uint32_t                        internal_refcount[4];
    ptl_sr_value_t                  regs[4][PTL_SR_LAST];
    ptl_internal_buffered_header_t *unexpecteds[4];
    ptl_internal_buffered_header_t *unexpecteds_buf[4];
#endif
} ptl_internal_nit_t;

extern ptl_internal_nit_t nit;

extern ptl_ni_limits_t    nit_limits[4];

extern uint32_t nit_limits_init[4];

#ifndef NO_ARG_VALIDATION
int INTERNAL  PtlInternalNIValidator(const ptl_internal_handle_converter_t ni);
#endif

#endif

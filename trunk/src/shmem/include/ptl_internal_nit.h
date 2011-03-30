#ifndef PTL_INTERNAL_NIT_H
#define PTL_INTERNAL_NIT_H

#include <stdint.h>                    /* for uint32_t */

#include "ptl_internal_handles.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_PT.h"
#include "ptl_internal_ints.h"

typedef struct {
    ptl_table_entry_t              *tables[4];
    uint32_t                        refcount[4];
    volatile uint32_t               internal_refcount[4];
    ptl_sr_value_t                  regs[4][2];
    ptl_internal_buffered_header_t *unexpecteds[4];
    ptl_internal_buffered_header_t *unexpecteds_buf[4];
} ptl_internal_nit_t;

extern ptl_internal_nit_t nit;
extern ptl_ni_limits_t    nit_limits[4];

int  PtlInternalNIValidator(const ptl_internal_handle_converter_t ni);
void PtlInternalDeallocUnexpectedHeader(ptl_internal_buffered_header_t *const hdr);

ptl_internal_buffered_header_t *PtlInternalAllocUnexpectedHeader(const uint_fast8_t ni);
#endif /* ifndef PTL_INTERNAL_NIT_H */
/* vim:set expandtab: */

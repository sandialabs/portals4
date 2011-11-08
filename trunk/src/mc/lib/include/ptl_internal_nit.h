#ifndef PTL_INTERNAL_NIT_H
#define PTL_INTERNAL_NIT_H

#include <stdint.h>                    /* for uint32_t */

#include "ptl_visibility.h"
#include "shared/ptl_internal_handles.h"

typedef struct {
    uint32_t                        refcount;
    uint32_t                        pid;
    uint32_t                        limits_refcount;
    ptl_sr_value_t                  status_registers[PTL_SR_LAST]; /* this will be updated by nic */
} ptl_internal_ni_t;

extern ptl_ni_limits_t nit_limits[4];

#ifndef NO_ARG_VALIDATION
int INTERNAL  PtlInternalNIValidator(const ptl_internal_handle_converter_t ni);
#endif

#endif

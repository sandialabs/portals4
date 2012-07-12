#ifndef PTL_INTERNAL_NIT_H
#define PTL_INTERNAL_NIT_H

#include <stdint.h>                    /* for uint32_t */

#include "ptl_visibility.h"
#include "shared/ptl_internal_handles.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_ME.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_PT.h"
#include "ptl_internal_triggered.h"


typedef struct {
    uint32_t                        refcount;
    uint32_t                        limits_refcount;

    void*                           shared_data;
    ptl_process_t*                  physical_address;
    ptl_rank_t*                     logical_address;
    ptl_sr_value_t*                 status_registers;
    ptl_internal_md_t              *i_md;
    ptl_shared_le_t                *i_le;
    ptl_shared_me_t                *i_me;
    ptl_internal_ct_t              *i_ct;
    ptl_internal_eq_t              *i_eq;
    ptl_internal_pt_t              *i_pt;
    ptl_shared_triggered_t         *i_triggered;
} ptl_internal_ni_t;

extern ptl_ni_limits_t nit_limits[4];

#ifndef NO_ARG_VALIDATION
int INTERNAL  PtlInternalNIValidator(const ptl_internal_handle_converter_t ni);
#endif

#endif

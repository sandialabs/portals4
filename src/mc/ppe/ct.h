
#ifndef MC_PPE_CT_H
#define MC_PPE_CT_H

#include "ppe/ppe.h"
#include "ppe/triggered.h"


static inline void PtlInternalCTSuccessInc(
        ptl_ppe_ni_t *ppe_ni, ptl_handle_ct_t ct_handle, ptl_size_t  increment)
{
    const ptl_internal_handle_converter_t ct_hc = {ct_handle};
    PPE_DBG("ct_index=%d value=%lu\n",ct_hc.s.code,increment);
    ppe_ni->client_ct[ct_hc.s.code].ct_event.success += increment;
    PtlInternalCTPullTriggers( ppe_ni, ct_handle );
}

static inline int
ct_set( ptl_ppe_ni_t *ppe_ni, int ct_index, ptl_ct_event_t new_event )
{
    const ptl_internal_handle_converter_t ct_hc = {.s.code = ct_index};

    PPE_DBG("ct_index=%d value=%lu\n",ct_index,new_event.success);

    ppe_ni->client_ct[ct_index].ct_event = new_event; 

    // PtlInternalCTPullTriggers only uses code field in ct_handle
    PtlInternalCTPullTriggers( ppe_ni, (ptl_handle_ct_t) ct_hc.a );
    return 0;
}


#endif

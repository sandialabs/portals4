
#ifndef MC_PPE_CT_H
#define MC_PPE_CT_H

#include "ppe/ppe.h"
#include "ppe/triggered.h"

int cancel_triggered( struct ptl_ppe_ni_t *ppe_ni, ptl_ppe_ct_t *ct );

static inline void 
ct_inc( ptl_ppe_ni_t *ppe_ni, ptl_ppe_handle_t ct_handle, ptl_size_t increment)
{
    PPE_DBG( "ct_index=%d value=%lu\n", ct_handle, increment );
    if ( ppe_ni->ppe_ct[ ct_handle ].in_use ) {
        ppe_ni->client_ct[ ct_handle ].ct_event.success += increment;
        ct_pull_triggers( ppe_ni, ct_handle );
    } else {
        fprintf(stderr,"%s():%d, unused ct %d\n",__func__,__LINE__,ct_handle);
    }
}


static inline void 
PtlInternalCTSuccessInc( ptl_ppe_ni_t *ppe_ni,
                        ptl_handle_ct_t ct_handle, ptl_size_t  increment)
{
    const ptl_internal_handle_converter_t ct_hc = {ct_handle};
    ct_inc( ppe_ni, ct_hc.s.code, increment );
}

static inline int
ct_set( ptl_ppe_ni_t *ppe_ni, ptl_ppe_handle_t ct_handle, 
                                ptl_ct_event_t new_event )
{
    PPE_DBG("ct_index=%d value=%lu\n", ct_handle, new_event.success );

    if ( ppe_ni->ppe_ct[ ct_handle ].in_use ) {
        ppe_ni->client_ct[ ct_handle ].ct_event = new_event; 
        ct_pull_triggers( ppe_ni, ct_handle );
    } else {
        fprintf(stderr,"%s():%d, unused ct %d\n",__func__,__LINE__,ct_handle);
    }
    return 0;
}


#endif

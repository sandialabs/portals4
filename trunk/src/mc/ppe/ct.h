
#ifndef MC_PPE_CT_H
#define MC_PPE_CT_H

static inline void PtlInternalCTSuccessInc(
        ptl_ppe_ni_t *ppe_ni, ptl_handle_ct_t ct_handle, ptl_size_t  increment)
{
    const ptl_internal_handle_converter_t ct_hc = {ct_handle};
    PPE_DBG("ct_index=%d value=%lu\n",ct_hc.s.code,increment);
    ppe_ni->client_ct[ct_hc.s.code].ct_event.success += increment;
}

static inline void PtlInternalCTPullTriggers(ptl_handle_ct_t ct_handle)
{
    PPE_DBG("\n");
}

static inline void PtlInternalCTTriggerCheck(ptl_handle_ct_t ct)
{
    PPE_DBG("\n");
}



static inline int
ct_inc( ptl_ppe_ni_t *ni, int ct_index, ptl_size_t value )
{
    PPE_DBG("ct_index=%d value=%lu\n",ct_index,value);
    ni->client_ct[ct_index].ct_event.success += value;
    return 0;
}

static inline int
ct_set( ptl_ppe_ni_t *ni, int ct_index, ptl_ct_event_t new_event )
{
    PPE_DBG("ct_index=%d value=%lu\n",ct_index,new_event.success);
    ni->client_ct[ct_index].ct_event = new_event; 
    return 0;
}


#endif

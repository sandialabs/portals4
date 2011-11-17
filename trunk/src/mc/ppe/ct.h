
#ifndef MC_PPE_CT_H
#define MC_PPE_CT_H

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

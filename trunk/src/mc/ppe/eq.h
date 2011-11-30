
#ifndef MC_PPE_EQ_H
#define MC_PPE_EQ_H

typedef uint16_t ptl_internal_uid_t;

static inline void PtlInternalEQPush( ptl_ppe_ni_t *ni,
                        ptl_handle_eq_t       handle,
                       ptl_internal_event_t *event)
{
    ptl_internal_handle_converter_t   eq_hc = { handle };

    PPE_DBG("ni=%p eq_handle=%#x event=%d\n", ni, eq_hc.a, event->type );

    ptl_circular_buffer_add_overwrite( 
                        ni->ppe_eq[eq_hc.s.code].xpmem_ptr->data, event );
}


static inline void PtlInternalEQPushESEND( ptl_ppe_ni_t *ni,
                                    const ptl_handle_eq_t eq_handle,
                                     const uint32_t        length,
                                     const uint64_t        roffset,
                                     void *const           user_ptr)
{
    ptl_event_t event;
    event.type          = PTL_EVENT_SEND;
    event.mlength       = length;
    event.remote_offset = roffset;
    event.user_ptr      = user_ptr;
    event.ni_fail_type  = PTL_NI_OK;
    PtlInternalEQPush( ni, eq_handle, &event );
}

#endif

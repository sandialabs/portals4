
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

static inline int
eq_write( ptl_ppe_ni_t *ni, int eq_index, ptl_event_t *event )
{
    PPE_DBG("eq_index=%d\n", eq_index );

    ptl_circular_buffer_add_overwrite( 
                        ni->ppe_eq[eq_index].xpmem_ptr->data, event );
    return 0;
}

#endif

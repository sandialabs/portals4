#include "config.h"

#include <sys/time.h>
#include <assert.h>

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_CT.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"


int
PtlCTAlloc(ptl_handle_ni_t  ni_handle,
           ptl_handle_ct_t *ct_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t    ct_hc = { .s.ni = ni.s.ni,
                                            .s.selector = HANDLE_CT_CODE  };
    ptl_internal_ct_t *ct;
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if (ct_handle == NULL) {
        VERBOSE_ERROR("passed in a NULL for ct_handle\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ct_hc.s.code = find_ct_index( ni.s.ni ); 
    if (ct_hc.s.code == -1 ) return PTL_FAIL;

    ct = get_ct(ct_hc.s.ni, ct_hc.s.code);
    memset(&ct->ct_event, 0, sizeof(ptl_ct_event_t));

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;

    entry->base.type         = PTLCTALLOC;
    entry->base.ni           = ni.s.ni;
    entry->base.remote_id    = ptl_iface_get_rank(&ptl_iface);
    entry->ctAlloc.ct_handle = ct_hc;

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                                  ptl_iface_get_peer(&ptl_iface),
                                  entry, sizeof(ptl_cqe_ctalloc_t));
    if (0 != ret) return PTL_FAIL;

    *ct_handle = ct_hc.a;

    return PTL_OK;
}


int
PtlCTFree(ptl_handle_ct_t ct_handle)
{
    ptl_internal_handle_converter_t ct_hc = { ct_handle };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }       
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif  

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;

    entry->base.type        = PTLCTFREE;
    entry->base.ni          = ct_hc.s.ni;
    entry->base.remote_id   = ptl_iface_get_rank(&ptl_iface);
    entry->ctFree.ct_handle = ct_hc;

    ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_ctfree_t));

    return PTL_OK;
}


int
PtlCTCancelTriggered(ptl_handle_ct_t ct_handle)
{
    ptl_internal_handle_converter_t ct_hc = { ct_handle };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }                                  
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;

    entry->base.type        = PTLCTCANCELTRIGGERED;
    entry->base.remote_id   = ptl_iface_get_rank(&ptl_iface);
    entry->ctFree.ct_handle = ct_hc;

    ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_ctcanceltriggered_t));

    return PTL_OK;
}


int
PtlCTGet(ptl_handle_ct_t ct_handle,
         ptl_ct_event_t *event)
{
    const ptl_internal_handle_converter_t ct_hc = { ct_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        return PTL_ARG_INVALID;
    }
#endif

    *event = get_ct(ct_hc.s.ni, ct_hc.s.code)->ct_event; 
    return PTL_OK;
}


int
PtlCTWait(ptl_handle_ct_t ct_handle,
          ptl_size_t      test,
          ptl_ct_event_t *event)
{
    //ptl_size_t  tests;
    unsigned int which;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        return PTL_ARG_INVALID;
    }
#endif
    
    return PtlCTPoll(&ct_handle, &test, 1, 
                     PTL_TIME_FOREVER, event, &which);
}


int
PtlCTPoll(const ptl_handle_ct_t *ct_handles,
          const ptl_size_t      *tests,
          unsigned int           size,
          ptl_time_t             timeout,
          ptl_ct_event_t        *event,
          unsigned int          *which)
{
    ptl_size_t ctidx;
    struct timeval end_time;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ct_handles == NULL) || (tests == NULL) || (size == 0)) {
        return PTL_ARG_INVALID;
    }
    for (ctidx = 0; ctidx < size; ctidx++) {
        if (PtlInternalCTHandleValidator(ct_handles[ctidx], 0)) {
            return PTL_ARG_INVALID;
        }
    }
    if (size > UINT32_MAX) {
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        return PTL_ARG_INVALID;
    }
    if (which == NULL) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    if( timeout != PTL_TIME_FOREVER ) {
        int retval = gettimeofday( &end_time, NULL ); 
        assert( retval == 0 );
        end_time.tv_sec += timeout / 1000;
        end_time.tv_usec += (timeout % 1000 ) * 1000;

        if ( end_time.tv_usec >= 1000000 ) {
            end_time.tv_usec -= 1000000;  
            ++end_time.tv_sec;
        }
    }

    do { 
        __sync_synchronize();
        for ( ctidx = 0; ctidx < size; ctidx++ ) {
            const ptl_internal_handle_converter_t ct_hc = { ct_handles[ctidx] };
            // should we read the values in place or copy them and read them?
            *event = get_ct( ct_hc.s.ni, ct_hc.s.code )->ct_event; 
            if ( event->failure || event->success >= tests[ctidx]  ) {
                *which = ctidx;
                return PTL_OK;
            } 
        }
        if ( timeout != PTL_TIME_FOREVER ) {
            struct timeval cur_time;
            int retval = gettimeofday( &cur_time, NULL ); 
            assert( retval == 0 );
            if ( cur_time.tv_sec >= end_time.tv_sec && 
                            cur_time.tv_usec >= end_time.tv_usec ) {
                return PTL_CT_NONE_REACHED;
            } 
        }
    } while ( 1 );
}

static inline int
ct_op( int type, ptl_handle_ct_t ct_handle, ptl_ct_event_t  ct_event,
             ptl_handle_ct_t trig_ct_handle, ptl_size_t trig_threshold )
{
    ptl_internal_handle_converter_t ct_hc = { ct_handle };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if ( type == PTLTRIGCTSET || type == PTLTRIGCTINC ) {
        const ptl_internal_handle_converter_t tct = { trig_ct_handle };
        if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
            return PTL_ARG_INVALID;
        }
        if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
            VERBOSE_ERROR("Triggered operations not allowed on this NI (%i);"
                            " max_triggered_ops set to zero\n", tct.s.ni);
            return PTL_ARG_INVALID;
        }
    }
#endif

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;

    entry->base.type            = type;
    entry->base.remote_id       = ptl_iface_get_rank(&ptl_iface);
    entry->base.ni              = ct_hc.s.ni;
    entry->ctOp.op.ct_handle    = ct_hc;
    entry->ctOp.op.ct_event     = ct_event;

    if ( type == PTLTRIGCTSET || type == PTLTRIGCTINC ) {
        entry->ctOp.triggered.ct_handle = 
                            (ptl_internal_handle_converter_t) trig_ct_handle;
        entry->ctOp.triggered.threshold = trig_threshold;
        entry->ctOp.triggered.index = find_triggered_index( ct_hc.s.ni ); 
        if ( entry->ctOp.triggered.index == -1 ) {
            ptl_cq_entry_free(ptl_iface_get_cq(&ptl_iface), entry);
            return PTL_FAIL; 
        }
    }

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                                  ptl_iface_get_peer(&ptl_iface), 
                                  entry, sizeof(ptl_cqe_ctop_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}

int PtlCTSet(ptl_handle_ct_t ct_h, ptl_ct_event_t  inc )
{
    return ct_op( PTLCTSET, ct_h, inc, PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredCTSet(ptl_handle_ct_t ct_h, ptl_ct_event_t  test,
                    ptl_handle_ct_t trig_ct_h, ptl_size_t threshold )
{
    return ct_op( PTLTRIGCTSET, ct_h, test, trig_ct_h, threshold  );
}

int PtlCTInc(ptl_handle_ct_t ct_h, ptl_ct_event_t  inc )
{
    return ct_op( PTLCTINC, ct_h, inc, PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredCTInc(ptl_handle_ct_t ct_h, ptl_ct_event_t  increment,
                    ptl_handle_ct_t trig_ct_h, ptl_size_t threshold )
{
    return ct_op( PTLTRIGCTINC, ct_h, increment, trig_ct_h, threshold  );
}

int INTERNAL
PtlInternalCTHandleValidator(ptl_handle_ct_t handle,
                             uint_fast8_t    none_ok)
{                                      /*{{{ */
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ct = { handle };
    if (ct.s.selector != HANDLE_CT_CODE) {
        VERBOSE_ERROR("Expected CT handle, but it's not a CT handle\n");
        return PTL_ARG_INVALID;
    }
    if (handle == PTL_CT_NONE) {
        if (none_ok == 1) {
            return PTL_OK;
        } else {
            VERBOSE_ERROR("PTL_CT_NONE not allowed here\n");
            return PTL_ARG_INVALID;
        }
    }
    if ((ct.s.ni > 3) || (ct.s.code > nit_limits[ct.s.ni].max_cts) ||
        (ptl_iface.ni[ct.s.ni].refcount == 0)) {
        VERBOSE_ERROR("CT NI too large (%u > 3) or code is wrong (%u > %u) "
                        "or nit table is uninitialized\n",
                      ct.s.ni, ct.s.code, nit_limits[ct.s.ni].max_cts);
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    return PTL_OK;
} 

/* vim:set expandtab: */


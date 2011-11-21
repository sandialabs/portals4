#include "config.h"

#include <sys/time.h>
#include <assert.h>

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_startup.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

int
PtlEQAlloc(ptl_handle_ni_t  ni_handle,
           ptl_size_t       count,
           ptl_handle_eq_t *eq_handle)
{
    int ret;
    const ptl_internal_handle_converter_t ni_hc  = { ni_handle };
    ptl_internal_handle_converter_t eq_hc  = { .s.ni = ni_hc.s.ni };
    ptl_internal_eq_t *eq;
    ptl_cqe_t *entry;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni_hc)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if (eq_handle == NULL) {
        VERBOSE_ERROR("passed in a NULL for eq_handle");
        return PTL_ARG_INVALID;
    }
    if (count > 0xffff) {
        VERBOSE_ERROR("insanely large count");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    /* Allocate an EQ index and a circular buffer behind the EQ */
    eq_hc.s.code = find_eq_index( ni_hc.s.ni);
    if ( eq_hc.s.code == -1 ) return PTL_FAIL;
    eq_hc.s.ni = ni_hc.s.ni;
    eq_hc.s.selector = HANDLE_EQ_CODE;

    eq = get_eq(eq_hc.s.ni, eq_hc.s.code);
    ret = ptl_circular_buffer_init(&eq->cb, count, sizeof(ptl_event_t));
    if (0 != ret) return PTL_FAIL;

    /* Send the information to the driver */
    ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    entry->base.type = PTLEQALLOC;
    entry->base.remote_id = ptl_iface_get_rank(&ptl_iface);
    entry->eqAlloc.eq_handle = eq_hc;
    entry->eqAlloc.cb = eq->cb;
    entry->eqAlloc.count = count;
    
    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                                  ptl_iface_get_peer(&ptl_iface), 
                                  entry, sizeof(ptl_cqe_eqalloc_t));
    if (0 != ret) return PTL_FAIL;

    printf("eqalloc: %ld\n", (long) eq_hc.a);

    *eq_handle = eq_hc.a;
    return PTL_OK;
}


int
PtlEQFree(ptl_handle_eq_t eq_handle)
{
    ptl_cqe_t *entry;
    ptl_circular_buffer_t *cb;
    int ret, cmd_ret = PTL_STATUS_LAST;
    ptl_internal_handle_converter_t eq_hc  = { eq_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 0)) {
        VERBOSE_ERROR("invalid EQ handle\n");
        return PTL_ARG_INVALID;
    }
#endif     

    cb = get_eq(eq_hc.s.ni, eq_hc.s.code)->cb;
        
    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;

    entry->base.type = PTLEQFREE;
    entry->base.remote_id  = ptl_iface_get_rank(&ptl_iface);
    entry->eqFree.eq_handle = eq_hc;
    entry->eqFree.retval_ptr = &cmd_ret;

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                                  ptl_iface_get_peer(&ptl_iface), 
                                  entry, sizeof(ptl_cqe_eqfree_t));
    if (ret < 0) return PTL_FAIL;

    /* wait for implementation to tell us we can free buffer.  Note
       that the implementation may set the in_use bit to zero before
       setting the retval, so need to have the pointer to the circular
       buffer stashed somewhere before sending the free command. */
    do {
        ret = ptl_ppe_progress(&ptl_iface, 1);
        if (ret < 0) return PTL_FAIL;
        __sync_synchronize();
    } while (PTL_STATUS_LAST == cmd_ret);

    ret = ptl_circular_buffer_fini(cb);
    if (ret < 0) return PTL_FAIL;

    return cmd_ret;
}


int
PtlEQGet(ptl_handle_eq_t eq_handle,
         ptl_event_t    *event)
{
    int ret, overwrite;
    ptl_internal_eq_t *eq;
    ptl_internal_handle_converter_t eq_hc  = { eq_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 0)) {
        VERBOSE_ERROR("invalid EQ handle\n");
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        VERBOSE_ERROR("null event\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    eq = get_eq(eq_hc.s.ni, eq_hc.s.code);

    ret = ptl_circular_buffer_get(eq->cb, event, &overwrite);
    if (ret < 0) return PTL_FAIL;
    if (ret == 1) return PTL_EQ_EMPTY;

    return (overwrite == 1) ? PTL_EQ_DROPPED : PTL_OK;
}


int
PtlEQWait(ptl_handle_eq_t eq_handle,
          ptl_event_t    *event)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 0)) {
        VERBOSE_ERROR("invalid EQ handle\n");
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        VERBOSE_ERROR("null event\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    unsigned int which;
    return PtlEQPoll( &eq_handle, 1, PTL_TIME_FOREVER, event, &which );
}


int
PtlEQPoll(const ptl_handle_eq_t *eq_handles,
          unsigned int           size,
          ptl_time_t             timeout,
          ptl_event_t           *event,
          unsigned int          *which)
{
    ptl_size_t eqidx; 
    struct timeval end_time;
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((event == NULL) && (which == NULL)) {  
        VERBOSE_ERROR("null event or null which\n");
        return PTL_ARG_INVALID;           
    }
    for (eqidx = 0; eqidx < size; ++eqidx) {
        if (PtlInternalEQHandleValidator(eq_handles[eqidx], 0)) {
            VERBOSE_ERROR("invalid EQ handle\n");
            return PTL_ARG_INVALID;
        }
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
        int ret, overwrite;
    
        for ( eqidx = 0; eqidx < size; eqidx++ ) {
            const ptl_internal_handle_converter_t eq_hc = { eq_handles[eqidx] };
            ptl_internal_eq_t *eq = get_eq(eq_hc.s.ni, eq_hc.s.code);
            ret = ptl_circular_buffer_get(eq->cb, event, &overwrite);
            if (ret == 0)  {
                return (overwrite == 1) ? PTL_EQ_DROPPED : PTL_OK;
            }
        }

        if ( timeout != PTL_TIME_FOREVER ) {
            struct timeval cur_time;
            int retval = gettimeofday( &cur_time, NULL );
            assert( retval == 0 );
            if ( cur_time.tv_sec >= end_time.tv_sec &&
                            cur_time.tv_usec >= end_time.tv_usec ) {
                return PTL_EQ_EMPTY;
            }  
        }
    } while ( 1 );   
}


#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalEQHandleValidator(ptl_handle_eq_t handle,
                                          int             none_ok)
{
    const ptl_internal_handle_converter_t eq = { handle };

    if (eq.s.selector != HANDLE_EQ_CODE) {
        VERBOSE_ERROR("selector not a EQ selector (%i)\n", eq.s.selector);
        return PTL_ARG_INVALID;
    }
    if ((none_ok == 1) && (handle == PTL_EQ_NONE)) {
        return PTL_OK;
    }
    if ((eq.s.ni > 3) || (eq.s.code > nit_limits[eq.s.ni].max_eqs) ||
        (ptl_iface.ni[eq.s.ni].refcount == 0)) {
        VERBOSE_ERROR("EQ NI too large (%u > 3) or code is wrong (%u > %u)"
                " or nit table is uninitialized\n",
                      eq.s.ni, eq.s.code, nit_limits[eq.s.ni].max_cts);
        return PTL_ARG_INVALID;
    }
    return PTL_OK;
}

#endif /* ifndef NO_ARG_VALIDATION */

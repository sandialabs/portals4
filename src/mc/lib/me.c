#include "config.h"

#include <assert.h>

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "shared/ptl_internal_handles.h"

#define ME_FREE      0
#define ME_ALLOCATED 1
#define ME_IN_USE    2

typedef struct {
    uint32_t                status; // 0=free, 1=allocated, 2=in-use
#if 0
    ptl_internal_appendME_t Qentry;
    ptl_me_t                visible;
    ptl_pt_index_t          pt_index;
    ptl_list_t              ptl_list;
#endif
} ptl_internal_me_t;

static ptl_internal_me_t *mes[4] = { NULL, NULL, NULL, NULL };


int PtlMEAppend(ptl_handle_ni_t  ni_handle,
                ptl_pt_index_t   pt_index,
                ptl_me_t        *me,
                ptl_list_t       ptl_list,
                void            *user_ptr,
                ptl_handle_me_t *me_handle)
{
    const ptl_internal_handle_converter_t ni     = { ni_handle };
    ptl_internal_handle_converter_t me_hc     = { .s.ni = ni.s.ni };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni == 1) || (ni.s.ni == 3)) { // must be a non-matching NI
        VERBOSE_ERROR("must be a matching NI\n");
        return PTL_ARG_INVALID;
    }
    if (nit.tables[ni.s.ni] == NULL) { // this should never happen
        assert(nit.tables[ni.s.ni] != NULL);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    {
        int ptv = PtlInternalPTValidate(&nit.tables[ni.s.ni][pt_index]);
        if ((ptv == 1) || (ptv == 3)) {    // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("MEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;        }
    }
#endif /* ifndef NO_ARG_VALIDATION */

    me_hc.s.selector =  get_my_ppe_rank();
    me_hc.s.code = find_me_index( ni.s.ni );

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLMEAPPEND;
    entry->u.meAppend.pt_index = pt_index;
    entry->u.meAppend.me = *me;
    entry->u.meAppend.list = ptl_list;
    entry->u.meAppend.user_ptr = user_ptr;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                    sizeof(ptl_cqe_t) );

    *me_handle = me_hc.a;

    return PTL_OK;
}

int PtlMEUnlink(ptl_handle_me_t me_handle)
{
    const ptl_internal_handle_converter_t         me_hc = { me_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized");
        return PTL_NO_INIT;
    }
    if ((me_hc.s.ni > 3) || 
        (me_hc.s.code > nit_limits[me_hc.s.ni].max_entries) ||
        (nit.refcount[me_hc.s.ni] == 0)) 
    {
        VERBOSE_ERROR("ME Handle has bad NI (%u > 3) or bad code (%u > %u)"
                        " or the NIT is uninitialized\n",
                me_hc.s.ni, me_hc.s.code, nit_limits[me_hc.s.ni].max_entries);
        return PTL_ARG_INVALID;
    }
    if (mes[me_hc.s.ni] == NULL) {
        VERBOSE_ERROR("ME array uninitialized\n");
        return PTL_ARG_INVALID;
    }
    __sync_synchronize();
    if (mes[me_hc.s.ni][me_hc.s.code].status == ME_FREE) {
        VERBOSE_ERROR("ME appears to be free already\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );
    
    entry->type = PTLMEUNLINK;
    entry->u.meUnlink.me_handle = me_hc;
    entry->u.meUnlink.me_handle.s.selector = get_my_ppe_rank();

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                    sizeof(ptl_cqe_t) );


    return PTL_OK;
}


int PtlMESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_me_t *me,
                ptl_search_op_t ptl_search_op,
                void           *user_ptr)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni == 0) || (ni.s.ni == 2)) { // must be a non-matching NI
        VERBOSE_ERROR("must be a non-matching NI\n");
        return PTL_ARG_INVALID;
    }
    if (nit.tables[ni.s.ni] == NULL) { // this should never happen
        assert(nit.tables[ni.s.ni] != NULL);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    {
        int ptv = PtlInternalPTValidate(&nit.tables[ni.s.ni][pt_index]);
        if ((ptv == 1) || (ptv == 3)) {    
            // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("LEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;
        }
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLESEARCH;
    entry->u.meSearch.ni_handle = ni;
    entry->u.meSearch.pt_index = pt_index;
    entry->u.meSearch.me = *me;
    entry->u.meSearch.ptl_search_op = ptl_search_op;
    entry->u.meSearch.user_ptr = user_ptr;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                    sizeof(ptl_cqe_t) );

    return PTL_OK;
}

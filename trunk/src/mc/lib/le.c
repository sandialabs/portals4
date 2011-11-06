#include "config.h"

#include <assert.h>

#include "portals4.h"
#include "ppe_if.h" 
        
#include "shared/ptl_internal_handles.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"

#define LE_FREE      0
#define LE_ALLOCATED 1
#define LE_IN_USE    2

typedef struct {
    uint32_t                status; // 0=free, 1=allocated, 2=in-use
#if 0
    ptl_internal_appendLE_t Qentry;
    ptl_le_t                visible;
    ptl_pt_index_t          pt_index;
    ptl_list_t              ptl_list;
#endif
} ptl_internal_le_t;


static ptl_internal_le_t *les[4] = { NULL, NULL, NULL, NULL };

int PtlLEAppend(ptl_handle_ni_t  ni_handle,
                ptl_pt_index_t   pt_index,
                const ptl_le_t  *le,
                ptl_list_t       ptl_list,
                void            *user_ptr,
                ptl_handle_le_t *le_handle)
{
    const ptl_internal_handle_converter_t ni     = { ni_handle };

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
        if ((ptv == 1) || (ptv == 3)) {    // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("LEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;
        }
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return PTL_OK;
}

int PtlLEUnlink(ptl_handle_le_t le_handle)
{
   const ptl_internal_handle_converter_t le = { le_handle };
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((le.s.ni > 3) || (le.s.code > nit_limits[le.s.ni].max_entries) ||
        (nit.refcount[le.s.ni] == 0)) {        VERBOSE_ERROR("LE Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n",
                      le.s.ni, le.s.code, nit_limits[le.s.ni].max_entries);
        return PTL_ARG_INVALID;
    }
    if (les[le.s.ni] == NULL) {
        VERBOSE_ERROR("LE array uninitialized\n");
        return PTL_ARG_INVALID;    }
    __sync_synchronize();
    if (les[le.s.ni][le.s.code].status == LE_FREE) {
        VERBOSE_ERROR("LE appears to be free already\n");
        return PTL_ARG_INVALID;
    }           
#endif /* ifndef NO_ARG_VALIDATION */
    return PTL_OK;
}

int PtlLESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_le_t *le,
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
        if ((ptv == 1) || (ptv == 3)) {    // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("LEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;
        }
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return PTL_OK;

}

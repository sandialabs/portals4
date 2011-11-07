#include "config.h"

#include "portals4.h"
#include "ppe_if.h" 
        
#include "shared/ptl_internal_handles.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_PT.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_MD.h"

#define MD_FREE   0
#define MD_IN_USE 1


typedef struct {
    uint32_t      in_use; // 0=free, 1=in_use
    ptl_md_t      visible;
#if 0
    uint_fast32_t refcount;
    uint8_t       pad1[16 - sizeof(uint32_t) - sizeof(uint_fast32_t)];
#ifdef REGISTER_ON_BIND
    uint64_t      xfe_handle;
    uint8_t       pad2[CACHELINE_WIDTH - (16 + sizeof(ptl_md_t) + sizeof(uint64_t))];
#else
    uint8_t       pad2[CACHELINE_WIDTH - (16 + sizeof(ptl_md_t))];
#endif
#endif
} ptl_internal_md_t ALIGNED (CACHELINE_WIDTH);

static ptl_internal_md_t *mds[4] = { NULL, NULL, NULL, NULL };



int PtlMDBind(ptl_handle_ni_t  ni_handle,
              const ptl_md_t  *md,
              ptl_handle_md_t *md_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t md_hc = { .s.selector = HANDLE_MD_CODE };

#ifndef NO_ARG_VALIDATION
    int ct_optional = 1;
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni > 3) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("ni is bad (%u > 3) or code invalid (%u != 0) or nit not initialized\n",                   ni.s.ni, ni.s.code);
        return PTL_ARG_INVALID; }
    /*if (md->start == NULL || md->length == 0) {
     * VERBOSE_ERROR("start is NULL (%p) or length is 0 (%u); cannot detect fail
ures!\n", md->start, (unsigned int)md->length);
     * return PTL_ARG_INVALID;
     * } */
    if (PtlInternalEQHandleValidator(md->eq_handle, 1)) {
        VERBOSE_ERROR("MD saw invalid EQ\n");
        return PTL_ARG_INVALID;
    }
    if (md->options & ~PTL_MD_OPTIONS_MASK) {
        VERBOSE_ERROR("Invalid options field passed to PtlMDBind (0x%x)\n", md->options);
        return PTL_ARG_INVALID;
    }
    if (md->options & (PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_REPLY |
                       PTL_MD_EVENT_CT_ACK)) {
        ct_optional = 0;
    }
    if (PtlInternalCTHandleValidator(md->ct_handle, ct_optional)) {
        VERBOSE_ERROR("MD saw invalid CT\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    int md_index = find_md_index( ni.s.ni );
    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLMDBIND;
    entry->u.mdBind.md_handle.s.ni       = ni.s.ni;
    entry->u.mdBind.md_handle.s.code     = md_index;
    entry->u.mdBind.md_handle.s.selector = get_my_id();

    entry->u.mdBind.md = *md;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                                    sizeof(ptl_cqe_t) );

    md_hc.s.code = md_index;
    md_hc.s.ni  = ni.s.ni;

    *md_handle = md_hc.a;

    return PTL_OK;
}

int PtlMDRelease(ptl_handle_md_t md_handle)
{
    ptl_internal_handle_converter_t md_hc = { md_handle };
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 0)) {
        VERBOSE_ERROR("MD handle invalid\n");
        return PTL_ARG_INVALID;
    }
#endif

    md_hc.s.selector = get_my_id();

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLMDRELEASE;
    entry->u.mdRelease.md_handle = md_hc;
    
    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, 
                        sizeof(ptl_cqe_t) );
    return PTL_OK;
}

#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalMDHandleValidator(ptl_handle_md_t handle,
                                          uint_fast8_t    care_about_ct)
{
    const ptl_internal_handle_converter_t md = { handle };
    ptl_internal_md_t                    *mdptr;
    
    if (md.s.selector != HANDLE_MD_CODE) {
        VERBOSE_ERROR("selector not a MD selector (%i)\n", md.s.selector);
        return PTL_ARG_INVALID;
    }
    if ((md.s.ni > 3) || (md.s.code > nit_limits[md.s.ni].max_mds) || (nit.refcount[md.s.ni] == 0)) {
        VERBOSE_ERROR("MD Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n",
                      md.s.ni, md.s.code, nit_limits[md.s.ni].max_mds);
        return PTL_ARG_INVALID;
    }
    if (mds[md.s.ni] == NULL) {
        VERBOSE_ERROR("MD aray for NI uninitialized\n");
        return PTL_ARG_INVALID;
    }
    __sync_synchronize();
    if (mds[md.s.ni][md.s.code].in_use != MD_IN_USE) {
        VERBOSE_ERROR("MD appears to be free already\n");
        return PTL_ARG_INVALID;
    }
    mdptr = &(mds[md.s.ni][md.s.code]);
    if (PtlInternalEQHandleValidator(mdptr->visible.eq_handle, 1)) {
        VERBOSE_ERROR("MD has a bad EQ handle\n");
        return PTL_ARG_INVALID;
    }
    if (care_about_ct) {
        int ct_optional = 1;
        if (mdptr->visible.options &
            (PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_REPLY | PTL_MD_EVENT_CT_ACK)) {                     ct_optional = 0;
        }
        if (PtlInternalCTHandleValidator(mdptr->visible.ct_handle, ct_optional))
 {          
            VERBOSE_ERROR("MD has a bad CT handle\n");
            return PTL_ARG_INVALID;
        }
    }
    return PTL_OK;                     
}

ptl_size_t INTERNAL PtlInternalMDLength(ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };

    return mds[md.s.ni][md.s.code].visible.length;
}

ptl_md_t INTERNAL *PtlInternalMDFetch(ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };

    /* this check allows us to process acks from/to dead NI's */
    if (mds[md.s.ni] != NULL) {
        return &(mds[md.s.ni][md.s.code].visible);
    } else {
        return NULL;
    }
}

#endif /* ifndef NO_ARG_VALIDATION */


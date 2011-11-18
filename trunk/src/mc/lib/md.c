#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"        
#include "ptl_internal_error.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_MD.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

int PtlMDBind(ptl_handle_ni_t  ni_handle,
              const ptl_md_t  *md,
              ptl_handle_md_t *md_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t md_hc = { .s.ni = ni.s.ni,
                                    .s.selector = HANDLE_MD_CODE };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    int ct_optional = 1;
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni > 3) || (ni.s.code != 0) || 
                    (ptl_iface.ni[ni.s.ni].refcount == 0)) 
    {
        VERBOSE_ERROR("ni is bad (%u > 3) or code invalid (%u != 0) or nit"
                                    " not initialized\n", ni.s.ni, ni.s.code);
        return PTL_ARG_INVALID; 
    }
    if (PtlInternalEQHandleValidator(md->eq_handle, 1)) {
        VERBOSE_ERROR("MD saw invalid EQ\n");
        return PTL_ARG_INVALID;
    }
    if (md->options & ~PTL_MD_OPTIONS_MASK) {
        VERBOSE_ERROR("Invalid options field passed to PtlMDBind (0x%x)\n", 
                                                                md->options);
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

    md_hc.s.code = find_md_index( ni.s.ni );
    if ( md_hc.s.code == - 1) return PTL_FAIL;

    ret = ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    if (0 != ret ) return PTL_FAIL;

    entry->base.type = PTLMDBIND;
    entry->base.remote_id   = ptl_iface_get_rank(&ptl_iface);
    entry->mdBind.md_handle = md_hc;
    entry->mdBind.md        = *md;

    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface),
                      entry, sizeof(ptl_cqe_mdbind_t));

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

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );

    entry->base.type = PTLMDRELEASE;
    entry->base.remote_id  = ptl_iface_get_rank(&ptl_iface);
    entry->mdRelease.md_handle = md_hc;
    
    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface),
                      entry, sizeof(ptl_cqe_mdrelease_t));

    return PTL_OK;
}

#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalMDHandleValidator(ptl_handle_md_t handle,
                                          uint_fast8_t    care_about_ct)
{
    const ptl_internal_handle_converter_t md = { handle };
    
    if (md.s.selector != HANDLE_MD_CODE) {
        VERBOSE_ERROR("selector not a MD selector (%i)\n", md.s.selector);
        return PTL_ARG_INVALID;
    }
    if ((md.s.ni > 3) || (md.s.code > nit_limits[md.s.ni].max_mds) ||
        (ptl_iface.ni[md.s.ni].refcount == 0)) {
        VERBOSE_ERROR("MD Handle has bad NI (%u > 3) or bad code (%u > %u)"
                                        " or the NIT is uninitialized\n",
                      md.s.ni, md.s.code, nit_limits[md.s.ni].max_mds);
        return PTL_ARG_INVALID;
    }

    __sync_synchronize();
    if ( ! md_is_inuse( md.s.ni, md.s.code ) ) {
        VERBOSE_ERROR("MD appears to be free already\n");
        return PTL_ARG_INVALID;
    }
    return PTL_OK;                     
}

#endif /* ifndef NO_ARG_VALIDATION */

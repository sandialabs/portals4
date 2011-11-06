#ifndef PPE_IF_MD_H
#define PPE_IF_MD_H

static inline int 
if_PtlMDBind( ptl_handle_ni_t    ni_handle,
               const ptl_md_t        *md,
                ptl_handle_md_t *md_handle )
{
    __DBG("\n");
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t md_hc = { .s.selector = HANDLE_MD_CODE };

    int md_index = find_md_index( ni.s.ni );
    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLMDBIND;
    entry->u.mdBind.md_handle.s.ni       = ni.s.ni;
    entry->u.mdBind.md_handle.s.code     = md_index;
    entry->u.mdBind.md_handle.s.selector = get_ppe_index();

    entry->u.mdBind.md = *md;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, sizeof(ptl_cqe_t) );

    md_hc.s.code = md_index; 
    md_hc.s.ni  = ni.s.ni; 

    *md_handle = md_hc.a;

    return PTL_OK;
}

static inline int 
if_PtlMDRelease( ptl_handle_md_t md_handle )
{
    __DBG("\n");
    ptl_internal_handle_converter_t md_hc = { md_handle };

    md_hc.s.selector = get_ppe_index();

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLMDRELEASE;
    entry->u.mdRelease.md_handle = md_hc;
    
    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, sizeof(ptl_cqe_t) );

    return PTL_OK;
}

#endif

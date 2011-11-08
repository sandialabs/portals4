#include "config.h"

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_pid.h"
#include "ptl_internal_nit.h"
#include "shared/ptl_internal_handles.h"

int PtlPut(ptl_handle_md_t  md_handle,
           ptl_size_t       local_offset,
           ptl_size_t       length,
           ptl_ack_req_t    ack_req,
           ptl_process_t    target_id,
           ptl_pt_index_t   pt_index,
           ptl_match_bits_t match_bits,
           ptl_size_t       remote_offset,
           void            *user_ptr,
           ptl_hdr_data_t   hdr_data)
{

    const ptl_internal_handle_converter_t md_hc = { md_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(md_handle) < local_offset + length) {
        VERBOSE_ERROR("MD too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(md_handle), local_offset + length);
        return PTL_ARG_INVALID;
    }
    switch (md_hc.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            if (PtlInternalLogicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            if (PtlInternalPhysicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (pid=%u, nid=%u)\n",
                              (unsigned int)target_id.phys.pid,
                              (unsigned int)target_id.phys.nid);
                return PTL_ARG_INVALID;
            }
            break;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && 
                (length > nit_limits[md_hc.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an"
                        " MD marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[md_hc.s.ni].max_volatile_size);
            return PTL_ARG_INVALID;
        }
    }
    if (pt_index > nit_limits[md_hc.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md_hc.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
#endif  /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t *entry;
        
    ptl_cq_entry_alloc( get_cq_handle(), &entry );
    
    entry->type = PTLPUT;
    entry->u.put.md_handle = md_hc; 
    entry->u.put.md_handle.s.selector = get_my_ppe_rank();
    entry->u.put.local_offset  = local_offset;
    entry->u.put.length        = length;
    entry->u.put.ack_req       = ack_req;
    entry->u.put.target_id     = target_id;
    entry->u.put.match_bits    = match_bits;
    entry->u.put.remote_offset = remote_offset;
    entry->u.put.user_ptr      = user_ptr;
    entry->u.put.hdr_data      = hdr_data; 
    
    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, 
                        sizeof(ptl_cqe_t) );

    return PTL_OK;
}

int PtlGet(ptl_handle_md_t  md_handle,
           ptl_size_t       local_offset,
           ptl_size_t       length,
           ptl_process_t    target_id,
           ptl_pt_index_t   pt_index,
           ptl_match_bits_t match_bits,
           ptl_size_t       remote_offset,
           void            *user_ptr)
{
    const ptl_internal_handle_converter_t md_hc = { md_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(md_handle) < local_offset + length) {
        VERBOSE_ERROR("MD too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(md_handle), local_offset + length);
        return PTL_ARG_INVALID;
    }
    switch (md_hc.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            if (PtlInternalLogicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            if (PtlInternalPhysicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (pid=%u, nid=%u)\n",
                              (unsigned int)target_id.phys.pid,
                              (unsigned int)target_id.phys.nid);
                return PTL_ARG_INVALID;
            }
            break;
    }
    if (pt_index > nit_limits[md_hc.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md_hc.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
#endif  /* ifndef NO_ARG_VALIDATION */
    ptl_cqe_t *entry;
        
    ptl_cq_entry_alloc( get_cq_handle(), &entry );
    
    entry->type = PTLGET;
    entry->u.get.md_handle = md_hc;
    entry->u.get.md_handle.s.selector = get_my_ppe_rank();
    entry->u.get.local_offset = local_offset; 
    entry->u.get.target_id = target_id; 
    entry->u.get.pt_index = pt_index; 
    entry->u.get.match_bits = match_bits; 
    entry->u.get.remote_offset = remote_offset; 
    entry->u.get.user_ptr = user_ptr; 
    
    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, 
                        sizeof(ptl_cqe_t) );
    return PTL_OK;
}

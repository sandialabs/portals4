#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_pid.h"
#include "ptl_internal_nit.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

static inline int ptl_put(int type, ptl_handle_md_t  md_handle,
           ptl_size_t       local_offset,
           ptl_size_t       length,
           ptl_ack_req_t    ack_req,
           ptl_process_t    target_id,
           ptl_pt_index_t   pt_index,
           ptl_match_bits_t match_bits,
           ptl_size_t       remote_offset,
           void            *user_ptr,
           ptl_hdr_data_t   hdr_data,
           ptl_handle_ct_t  trig_ct_handle,
           ptl_size_t       trig_threshold  )
{
    const ptl_internal_handle_converter_t md_hc = { md_handle };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid md_handle\n");
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
    if ( type == PTLTRIGPUT ) {
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
#endif  /* ifndef NO_ARG_VALIDATION */

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;
    
    entry->base.type              = type;
    entry->base.remote_id         = ptl_iface_get_rank(&ptl_iface);
    entry->base.ni                = md_hc.s.ni;
    entry->put.args.md_handle     = md_hc; 
    entry->put.args.local_offset  = local_offset;
    entry->put.args.length        = length;
    entry->put.args.ack_req       = ack_req;
    entry->put.args.target_id     = target_id;
    entry->put.args.match_bits    = match_bits;
    entry->put.args.remote_offset = remote_offset;
    entry->put.args.user_ptr      = user_ptr;
    entry->put.args.pt_index      = pt_index;
    entry->put.args.hdr_data      = hdr_data; 

    if ( type == PTLTRIGPUT ) {
        entry->put.triggered_args.ct_handle = 
                            (ptl_internal_handle_converter_t) trig_ct_handle;
        entry->put.triggered_args.threshold = trig_threshold;
        entry->put.triggered_args.index = find_triggered_index( md_hc.s.ni ); 
        if ( entry->put.triggered_args.index == -1 ) {
            ptl_cq_entry_free(ptl_iface_get_cq(&ptl_iface), entry);
            return PTL_FAIL; 
        }
    }
    
    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface),
                                  entry, sizeof(ptl_cqe_put_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}

static inline int
ptl_get(int type,
       ptl_handle_md_t  md_handle,
       ptl_size_t       local_offset,
       ptl_size_t       length,
       ptl_process_t    target_id,
       ptl_pt_index_t   pt_index,
       ptl_match_bits_t match_bits,
       ptl_size_t       remote_offset,
       void            *user_ptr,
       ptl_handle_ct_t  trig_ct_handle,
       ptl_size_t       trig_threshold )
{
    const ptl_internal_handle_converter_t md_hc = { md_handle };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid md_handle\n");
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
    if ( type == PTLTRIGGET ) {
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
#endif  /* ifndef NO_ARG_VALIDATION */
        
    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;
    
    entry->base.type = type;
    entry->base.remote_id = ptl_iface_get_rank(&ptl_iface);
    entry->get.args.md_handle     = md_hc;
    entry->get.args.local_offset  = local_offset; 
    entry->get.args.length        = length;
    entry->get.args.target_id     = target_id; 
    entry->get.args.pt_index      = pt_index; 
    entry->get.args.match_bits    = match_bits; 
    entry->get.args.remote_offset = remote_offset; 
    entry->get.args.user_ptr      = user_ptr; 

    if ( type == PTLTRIGGET ) {
        entry->get.triggered_args.ct_handle = 
                            (ptl_internal_handle_converter_t) trig_ct_handle;
        entry->get.triggered_args.threshold = trig_threshold;
        entry->get.triggered_args.index = find_triggered_index( md_hc.s.ni ); 
        if ( entry->get.triggered_args.index == -1 ) {
            ptl_cq_entry_free(ptl_iface_get_cq(&ptl_iface), entry);
            return PTL_FAIL; 
        }
    }
    
    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface), 
                                  entry, sizeof(ptl_cqe_get_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}

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
    return ptl_put( PTLPUT, md_handle, local_offset, length, ack_req,
        target_id, pt_index, match_bits, remote_offset, user_ptr, hdr_data,
                        PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredPut(ptl_handle_md_t  md_handle,
                    ptl_size_t       local_offset,
                    ptl_size_t       length,
                    ptl_ack_req_t    ack_req,
                    ptl_process_t    target_id,
                    ptl_pt_index_t   pt_index,
                    ptl_match_bits_t match_bits,
                    ptl_size_t       remote_offset,
                    void            *user_ptr,
                    ptl_hdr_data_t   hdr_data,
                    ptl_handle_ct_t  trig_ct_handle,
                    ptl_size_t       threshold)
{
    return ptl_put( PTLTRIGPUT, md_handle, local_offset, length, ack_req,
        target_id, pt_index, match_bits, remote_offset, user_ptr, hdr_data,
                        trig_ct_handle, threshold);
}

int
PtlGet( ptl_handle_md_t  md_handle,
       ptl_size_t       local_offset,
       ptl_size_t       length,
       ptl_process_t    target_id,
       ptl_pt_index_t   pt_index,
       ptl_match_bits_t match_bits,
       ptl_size_t       remote_offset,
       void            *user_ptr )
{
    return ptl_get( PTLGET, md_handle, local_offset, length, target_id, pt_index, 
                        match_bits, remote_offset, user_ptr, PTL_INVALID_HANDLE, 0 );
}

int
PtlTriggeredGet(ptl_handle_md_t  md_handle,
       ptl_size_t       local_offset,
       ptl_size_t       length,
       ptl_process_t    target_id,
       ptl_pt_index_t   pt_index,
       ptl_match_bits_t match_bits,
       ptl_size_t       remote_offset,
       void            *user_ptr,
       ptl_handle_ct_t  trig_ct_handle,
       ptl_size_t       trig_threshold )
{
    return ptl_get( PTLTRIGGET, md_handle, local_offset, length, target_id, pt_index, 
              match_bits, remote_offset, user_ptr, trig_ct_handle, trig_threshold );
}


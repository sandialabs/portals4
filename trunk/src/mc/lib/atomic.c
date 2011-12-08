#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_global.h"
#include "ptl_internal_pid.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"


static inline int
ptl_atomic(int type, ptl_handle_md_t  md_handle,
          ptl_size_t       local_offset,
          ptl_size_t       length,
          ptl_ack_req_t    ack_req,
          ptl_process_t    target_id,
          ptl_pt_index_t   pt_index,
          ptl_match_bits_t match_bits,
          ptl_size_t       remote_offset,
          void            *user_ptr,
          ptl_hdr_data_t   hdr_data,
          ptl_op_t         operation,
          ptl_datatype_t   datatype,
          ptl_handle_ct_t  trig_ct_handle,
          ptl_size_t       trig_threshold )
{
    const ptl_internal_handle_converter_t md_hc         = { md_handle };
    int ret;
    ptl_cqe_t   *entry;
    
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }   
    if (length > nit_limits[md_hc.s.ni].max_atomic_size) {
        VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[md_hc.s.ni].max_atomic_size);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid MD\n");
        return PTL_ARG_INVALID;
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_INT8_T:
            case PTL_UINT8_T:
                multiple = 1;
                break;
            case PTL_INT16_T:
            case PTL_UINT16_T:
                multiple = 2;
                break;
            case PTL_INT32_T:
            case PTL_UINT32_T:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_INT64_T:
            case PTL_UINT64_T:
            case PTL_DOUBLE:
            case PTL_FLOAT_COMPLEX:
                multiple = 8;
                break;
            case PTL_LONG_DOUBLE:
            case PTL_DOUBLE_COMPLEX:
                multiple = 16;
                break;
            case PTL_LONG_DOUBLE_COMPLEX:
                multiple = 32;
                break;
        }
        if (length % multiple != 0) {
            VERBOSE_ERROR("Length not a multiple of datatype size\n");
            return PTL_ARG_INVALID;
        }
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
    switch (operation) {
        case PTL_SWAP:
        case PTL_CSWAP:
        case PTL_MSWAP:
            VERBOSE_ERROR("SWAP/CSWAP/MSWAP invalid optypes for PtlAtomic()\n");
            return PTL_ARG_INVALID;

        case PTL_LOR:
        case PTL_LAND:
        case PTL_LXOR:
        case PTL_BOR:
        case PTL_BAND:
        case PTL_BXOR:
            switch (datatype) {
                case PTL_DOUBLE:
                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for"
                                            " logical/binary operations\n");
                    return PTL_ARG_INVALID;

                default:
                    break;
            }
        default:
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
    if ( type == PTLTRIGATOMIC ) {
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
    entry->atomic.args.md_handle       = md_hc;
    entry->atomic.args.local_offset    = local_offset;
    entry->atomic.args.length          = length;
    entry->atomic.args.ack_req         = ack_req;
    entry->atomic.args.target_id       = target_id;
    entry->atomic.args.pt_index        = pt_index;
    entry->atomic.args.match_bits      = match_bits;
    entry->atomic.args.remote_offset   = remote_offset;
    entry->atomic.args.user_ptr        = user_ptr;
    entry->atomic.args.hdr_data        = hdr_data;
    entry->atomic.atomic_args.operation       = operation;
    entry->atomic.atomic_args.datatype        = datatype;

    if ( type == PTLTRIGATOMIC ) {
        entry->atomic.triggered_args.ct_handle = 
                            (ptl_internal_handle_converter_t) trig_ct_handle;
        entry->atomic.triggered_args.threshold = trig_threshold;
        entry->atomic.triggered_args.index = find_triggered_index( md_hc.s.ni );
        if ( entry->atomic.triggered_args.index == -1 ) {
            ptl_cq_entry_free(ptl_iface_get_cq(&ptl_iface), entry);
            return PTL_FAIL; 
        }
    }                 

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface),
                                  entry, sizeof(ptl_cqe_atomic_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}


static inline int
ptl_fetch_atomic( int type, ptl_handle_md_t  get_md_handle,
               ptl_size_t       local_get_offset,
               ptl_handle_md_t  put_md_handle,
               ptl_size_t       local_put_offset,
               ptl_size_t       length,
               ptl_process_t    target_id,
               ptl_pt_index_t   pt_index,
               ptl_match_bits_t match_bits,
               ptl_size_t       remote_offset,
               void            *user_ptr,
               ptl_hdr_data_t   hdr_data,
               ptl_op_t         operation,
               ptl_datatype_t   datatype,
               ptl_handle_ct_t  trig_ct_handle,
               ptl_size_t       trig_threshold )
{
    const ptl_internal_handle_converter_t get_md     = { get_md_handle };
    const ptl_internal_handle_converter_t put_md     = { put_md_handle };
    ptl_cqe_t   *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(get_md_handle, 1)) {
        VERBOSE_ERROR("Invalid get_md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(put_md_handle, 1)) {
        VERBOSE_ERROR("Invalid put_md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (length > nit_limits[get_md.s.ni].max_atomic_size) {
        VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[get_md.s.ni].max_atomic_size);
        return PTL_ARG_INVALID;
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_INT8_T:
            case PTL_UINT8_T:
                multiple = 1;
                break;
            case PTL_INT16_T:
            case PTL_UINT16_T:
                multiple = 2;
                break;
            case PTL_INT32_T:
            case PTL_UINT32_T:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_INT64_T:
            case PTL_UINT64_T:
            case PTL_DOUBLE:
            case PTL_FLOAT_COMPLEX:
                multiple = 8;
                break;
            case PTL_LONG_DOUBLE:
            case PTL_DOUBLE_COMPLEX:
                multiple = 16;
                break;
            case PTL_LONG_DOUBLE_COMPLEX:
                multiple = 32;
                break;
        }
        if (length % multiple != 0) {
            VERBOSE_ERROR("Length not a multiple of datatype size\n");
            return PTL_ARG_INVALID;
        }
    }
    if (get_md.s.ni != put_md.s.ni) {
        VERBOSE_ERROR("MDs *must* be on the same NI\n");
        return PTL_ARG_INVALID;
    }
    switch (get_md.s.ni) {
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
    switch (operation) {
        case PTL_CSWAP:
        case PTL_MSWAP:
            VERBOSE_ERROR("MSWAP/CSWAP should be performed with PtlSwap\n");
            return PTL_ARG_INVALID;

        case PTL_LOR:
        case PTL_LAND:
        case PTL_LXOR:
        case PTL_BOR:
        case PTL_BAND:
        case PTL_BXOR:
            switch (datatype) {
                case PTL_DOUBLE:
                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for logical/binary operations\n");
                    return PTL_ARG_INVALID;

                default:
                    break;
            }
        default:
            break;
    }
    if (pt_index > nit_limits[get_md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[get_md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_put_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (local_get_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if ( type == PTLTRIGFETCHATOMIC) {
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
    entry->fetchAtomic.args.cmd_get_md_handle =  get_md;  
    entry->fetchAtomic.args.cmd_local_get_offset = local_get_offset;
    entry->fetchAtomic.args.cmd_put_md_handle    = put_md;
    entry->fetchAtomic.args.cmd_local_put_offset = local_put_offset;
    entry->fetchAtomic.args.length           = length;
    entry->fetchAtomic.args.target_id        = target_id;
    entry->fetchAtomic.args.pt_index         = pt_index;
    entry->fetchAtomic.args.match_bits       = match_bits;
    entry->fetchAtomic.args.remote_offset    = remote_offset;
    entry->fetchAtomic.args.user_ptr         = user_ptr;
    entry->fetchAtomic.args.hdr_data         = hdr_data;
    entry->fetchAtomic.atomic_args.operation = operation;
    entry->fetchAtomic.atomic_args.datatype  = datatype;

    if ( type == PTLTRIGFETCHATOMIC ) {
        entry->fetchAtomic.triggered_args.ct_handle = 
                            (ptl_internal_handle_converter_t) trig_ct_handle;
        entry->fetchAtomic.triggered_args.threshold = trig_threshold;
        entry->fetchAtomic.triggered_args.index = find_triggered_index( get_md.s.ni );
        if ( entry->fetchAtomic.triggered_args.index == -1 ) {
            ptl_cq_entry_free(ptl_iface_get_cq(&ptl_iface), entry);
            return PTL_FAIL; 
        }
    }                 

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface), 
                                  entry, sizeof(ptl_cqe_fetchatomic_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}


static inline int
ptl_swap(int type, ptl_handle_md_t  get_md_handle,
        ptl_size_t       local_get_offset,
        ptl_handle_md_t  put_md_handle,
        ptl_size_t       local_put_offset,
        ptl_size_t       length,
        ptl_process_t    target_id,
        ptl_pt_index_t   pt_index,
        ptl_match_bits_t match_bits,
        ptl_size_t       remote_offset,
        void            *user_ptr,
        ptl_hdr_data_t   hdr_data,
        const void      *operand,
        ptl_op_t         operation,
        ptl_datatype_t   datatype,
        ptl_handle_ct_t  trig_ct_handle,
        ptl_size_t       trig_threshold )
{
    const ptl_internal_handle_converter_t get_md_hc = { get_md_handle };
    const ptl_internal_handle_converter_t put_md_hc = { put_md_handle };
    ptl_cqe_t   *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t put_md = { put_md_handle };
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(get_md_handle, 1)) {
        VERBOSE_ERROR("Swap saw invalid get_md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(put_md_handle, 1)) {
        VERBOSE_ERROR("Swap saw invalid put_md_handle\n");
        return PTL_ARG_INVALID;
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_INT8_T:
            case PTL_UINT8_T:
                multiple = 1;
                break;
            case PTL_INT16_T:
            case PTL_UINT16_T:
                multiple = 2;
                break;
            case PTL_INT32_T:
            case PTL_UINT32_T:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_INT64_T:
            case PTL_UINT64_T:
            case PTL_DOUBLE:
            case PTL_FLOAT_COMPLEX:
                multiple = 8;
                break;
            case PTL_LONG_DOUBLE:
            case PTL_DOUBLE_COMPLEX:
                multiple = 16;
                break;
            case PTL_LONG_DOUBLE_COMPLEX:
                multiple = 32;
                break;
        }
        if (length % multiple != 0) {
            VERBOSE_ERROR("Length not a multiple of datatype size\n");
            return PTL_ARG_INVALID;
        }
    }
    if (get_md_hc.s.ni != put_md.s.ni) {
        VERBOSE_ERROR("MDs *must* be on the same NI\n");
        return PTL_ARG_INVALID;
    }
    switch (get_md_hc.s.ni) {
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
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
    }
    switch (operation) {
        case PTL_SWAP:                 
            if (length > nit_limits[get_md_hc.s.ni].max_atomic_size) {
                VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size"
                                                                    " (%u)\n",  
                      (unsigned int)length,
                      (unsigned int)nit_limits[get_md_hc.s.ni].max_atomic_size);
                return PTL_ARG_INVALID; 
            }
            break;        case PTL_CSWAP:
        case PTL_MSWAP:
            if (length > 32) {
                VERBOSE_ERROR("Length (%u) is bigger than one datatype (32)\n",
                              (unsigned int)length);
                return PTL_ARG_INVALID;            }
            switch (datatype) {
                case PTL_DOUBLE:                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes"
                                                        " for CSWAP/MSWAP\n");
                    return PTL_ARG_INVALID;    
                default:                    break;
            }         
            break;
        default:
            VERBOSE_ERROR("Only PTL_SWAP/CSWAP/MSWAP may be used with"
                                                                " PtlSwap\n");  
            return PTL_ARG_INVALID;
    }   
    if (pt_index > nit_limits[get_md_hc.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[get_md_hc.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }       
    if (local_put_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }   
    if (local_get_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }           
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }           
    if ( type == PTLTRIGSWAP ) {
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
    entry->swap.args.cmd_get_md_handle =  get_md_hc;  
    entry->swap.args.cmd_local_get_offset = local_get_offset;
    entry->swap.args.cmd_put_md_handle    = put_md_hc;
    entry->swap.args.cmd_local_put_offset = local_put_offset;
    entry->swap.args.length           = length;
    entry->swap.args.target_id        = target_id;
    entry->swap.args.pt_index         = pt_index;
    entry->swap.args.match_bits       = match_bits;
    entry->swap.args.remote_offset    = remote_offset;
    entry->swap.args.user_ptr         = user_ptr;
    entry->swap.args.hdr_data         = hdr_data;
    entry->swap.atomic_args.operand   = operand;
    entry->swap.atomic_args.operation = operation;
    entry->swap.atomic_args.datatype  = datatype;

    if ( type == PTLTRIGSWAP ) {
        entry->swap.triggered_args.ct_handle = 
                            (ptl_internal_handle_converter_t) trig_ct_handle;
        entry->swap.triggered_args.threshold = trig_threshold;
        entry->swap.triggered_args.index = find_triggered_index( get_md_hc.s.ni );
        if ( entry->swap.triggered_args.index == -1 ) {
            ptl_cq_entry_free(ptl_iface_get_cq(&ptl_iface), entry);
            return PTL_FAIL; 
        }
    }                 

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface),
                                  entry, sizeof(ptl_cqe_swap_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}

int PtlAtomic(ptl_handle_md_t  md_handle,
                       ptl_size_t       local_offset,
                       ptl_size_t       length,
                       ptl_ack_req_t    ack_req,
                       ptl_process_t    target_id,
                       ptl_pt_index_t   pt_index,
                       ptl_match_bits_t match_bits,
                       ptl_size_t       remote_offset,
                       void            *user_ptr,
                       ptl_hdr_data_t   hdr_data,
                       ptl_op_t         operation,
                       ptl_datatype_t   datatype )
{
    return ptl_atomic( PTLATOMIC, md_handle, local_offset, length, ack_req,
        target_id, pt_index, match_bits, remote_offset, user_ptr, hdr_data, 
        operation, datatype, PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredAtomic(ptl_handle_md_t  md_handle,
                       ptl_size_t       local_offset,
                       ptl_size_t       length,
                       ptl_ack_req_t    ack_req,
                       ptl_process_t    target_id,
                       ptl_pt_index_t   pt_index,
                       ptl_match_bits_t match_bits,
                       ptl_size_t       remote_offset,
                       void            *user_ptr,
                       ptl_hdr_data_t   hdr_data,
                       ptl_op_t         operation,
                       ptl_datatype_t   datatype,
                       ptl_handle_ct_t  trig_ct_handle,
                       ptl_size_t       trig_threshold)
{
    return ptl_atomic( PTLTRIGATOMIC, md_handle, local_offset, length, ack_req,
        target_id, pt_index, match_bits, remote_offset, user_ptr, hdr_data,
        operation, datatype, trig_ct_handle, trig_threshold );
}

int PtlFetchAtomic(ptl_handle_md_t  get_md_handle,
                            ptl_size_t       local_get_offset,
                            ptl_handle_md_t  put_md_handle,
                            ptl_size_t       local_put_offset,
                            ptl_size_t       length,
                            ptl_process_t    target_id,
                            ptl_pt_index_t   pt_index,
                            ptl_match_bits_t match_bits,
                            ptl_size_t       remote_offset,
                            void            *user_ptr,
                            ptl_hdr_data_t   hdr_data,
                            ptl_op_t         operation,
                            ptl_datatype_t   datatype )
{
    return ptl_fetch_atomic( PTLFETCHATOMIC, get_md_handle, local_get_offset,
        put_md_handle, local_put_offset, length, target_id, pt_index, 
        match_bits, remote_offset, user_ptr, hdr_data, operation, datatype,
        PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredFetchAtomic(ptl_handle_md_t  get_md_handle,
                            ptl_size_t       local_get_offset,
                            ptl_handle_md_t  put_md_handle,
                            ptl_size_t       local_put_offset,
                            ptl_size_t       length,
                            ptl_process_t    target_id,
                            ptl_pt_index_t   pt_index,
                            ptl_match_bits_t match_bits,
                            ptl_size_t       remote_offset,
                            void            *user_ptr,
                            ptl_hdr_data_t   hdr_data,
                            ptl_op_t         operation,
                            ptl_datatype_t   datatype,
                            ptl_handle_ct_t  trig_ct_handle,
                            ptl_size_t       trig_threshold)
{
    return ptl_fetch_atomic( PTLTRIGFETCHATOMIC, get_md_handle,
        local_get_offset, put_md_handle, local_put_offset, length, target_id,
        pt_index, match_bits, remote_offset, user_ptr, hdr_data, operation,
        datatype, trig_ct_handle, trig_threshold );
}

int PtlSwap(ptl_handle_md_t  get_md_handle,
                     ptl_size_t       local_get_offset,
                     ptl_handle_md_t  put_md_handle,
                     ptl_size_t       local_put_offset,
                     ptl_size_t       length,
                     ptl_process_t    target_id,
                     ptl_pt_index_t   pt_index,
                     ptl_match_bits_t match_bits,
                     ptl_size_t       remote_offset,
                     void            *user_ptr,
                     ptl_hdr_data_t   hdr_data,
                     const void      *operand,
                     ptl_op_t         operation,
                     ptl_datatype_t   datatype )
{
    return ptl_swap( PTLSWAP, get_md_handle,
        local_get_offset, put_md_handle, local_put_offset, length, target_id,
        pt_index, match_bits, remote_offset, user_ptr, hdr_data, operand,
        operation, datatype, PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredSwap(ptl_handle_md_t  get_md_handle,
                     ptl_size_t       local_get_offset,
                     ptl_handle_md_t  put_md_handle,
                     ptl_size_t       local_put_offset,
                     ptl_size_t       length,
                     ptl_process_t    target_id,
                     ptl_pt_index_t   pt_index,
                     ptl_match_bits_t match_bits,
                     ptl_size_t       remote_offset,
                     void            *user_ptr,
                     ptl_hdr_data_t   hdr_data,
                     const void      *operand,
                     ptl_op_t         operation,
                     ptl_datatype_t   datatype,
                     ptl_handle_ct_t  trig_ct_handle,
                     ptl_size_t       trig_threshold)
{
    return ptl_swap( PTLTRIGSWAP, get_md_handle,
        local_get_offset, put_md_handle, local_put_offset, length, target_id,
        pt_index, match_bits, remote_offset, user_ptr, hdr_data, operand,
        operation, datatype, trig_ct_handle, trig_threshold );
}

int
PtlAtomicSync(void)
{
    ptl_cqe_t   *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;

    entry->base.type = PTLATOMICSYNC;
    entry->base.remote_id = ptl_iface_get_rank(&ptl_iface);

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface), 
                                  entry, sizeof(ptl_cqe_atomicsync_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}

/* vim:set expandtab */

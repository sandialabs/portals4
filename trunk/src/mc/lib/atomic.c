#include "config.h"

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_pid.h"
#include "shared/ptl_internal_handles.h"

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
              ptl_datatype_t   datatype)
{
    const ptl_internal_handle_converter_t md_hc         = { md_handle };
    
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
#endif  /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t   *entry;
    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLATOMIC;
    entry->u.atomic.md_handle       = md_hc;
    entry->u.atomic.md_handle.s.selector = get_my_ppe_rank();
    entry->u.atomic.local_offset    = local_offset;
    entry->u.atomic.length          = length;
    entry->u.atomic.ack_req         = ack_req;
    entry->u.atomic.target_id       = target_id;
    entry->u.atomic.pt_index        = pt_index;
    entry->u.atomic.match_bits      = match_bits;
    entry->u.atomic.remote_offset   = remote_offset;
    entry->u.atomic.user_ptr        = user_ptr;
    entry->u.atomic.hdr_data        = hdr_data;
    entry->u.atomic.operation       = operation;
    entry->u.atomic.datatype        = datatype;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                    sizeof(ptl_cqe_t) );

    return PTL_OK;
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
                   ptl_datatype_t   datatype)
{
    const ptl_internal_handle_converter_t get_md     = { get_md_handle };
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t put_md = { put_md_handle };
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
    if (PtlInternalMDLength(get_md_handle) < local_get_offset + length) {
        VERBOSE_ERROR("FetchAtomic saw get_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(get_md_handle), local_get_offset + length);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(put_md_handle) < local_put_offset + length) {
        VERBOSE_ERROR("FetchAtomic saw put_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(put_md_handle), local_put_offset + length);
        return PTL_ARG_INVALID;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(put_md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && (length > nit_limits[put_md.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an MD marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[put_md.s.ni].max_volatile_size);
            return PTL_ARG_INVALID;
        }
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
#endif  /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t   *entry;
    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLFETCHATOMIC;
    entry->u.fetchAtomic.get_md_handle =  get_md;  
    entry->u.fetchAtomic.get_md_handle.s.selector = get_my_ppe_rank();  
    entry->u.fetchAtomic.local_get_offset = local_get_offset;
    entry->u.fetchAtomic.put_md_handle    = (ptl_internal_handle_converter_t) put_md_handle; 
    entry->u.fetchAtomic.put_md_handle.s.selector = get_my_ppe_rank();
    entry->u.fetchAtomic.length           = length;
    entry->u.fetchAtomic.target_id        = target_id;
    entry->u.fetchAtomic.pt_index         = pt_index;
    entry->u.fetchAtomic.match_bits       = match_bits;
    entry->u.fetchAtomic.remote_offset    = remote_offset;
    entry->u.fetchAtomic.user_ptr         = user_ptr;
    entry->u.fetchAtomic.hdr_data         = hdr_data;
    entry->u.fetchAtomic.operation        = operation;
    entry->u.fetchAtomic.datatype         = datatype;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                    sizeof(ptl_cqe_t) );

    return PTL_OK;
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
            ptl_datatype_t   datatype)
{
    const ptl_internal_handle_converter_t get_md_hc = { get_md_handle };

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
    if (PtlInternalMDLength(get_md_handle) < local_get_offset + length) {
        VERBOSE_ERROR("Swap saw get_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(get_md_handle), 
                                            local_get_offset + length);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(put_md_handle) < local_put_offset + length) {
        VERBOSE_ERROR("Swap saw put_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(put_md_handle), 
                                            local_put_offset + length);
        return PTL_ARG_INVALID;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(put_md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && (length > nit_limits[put_md.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an MD"
                    " marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[put_md.s.ni].max_volatile_size);
            return PTL_ARG_INVALID;
        }
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
#endif  /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t   *entry;
    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLSWAP;
    entry->u.swap.get_md_handle =  get_md_hc;  
    entry->u.swap.get_md_handle.s.selector = get_my_ppe_rank();  
    entry->u.swap.local_get_offset = local_get_offset;
    entry->u.swap.put_md_handle    = (ptl_internal_handle_converter_t) put_md_handle; 
    entry->u.swap.put_md_handle.s.selector = get_my_ppe_rank();
    entry->u.swap.length           = length;
    entry->u.swap.target_id        = target_id;
    entry->u.swap.pt_index         = pt_index;
    entry->u.swap.match_bits       = match_bits;
    entry->u.swap.remote_offset    = remote_offset;
    entry->u.swap.user_ptr         = user_ptr;
    entry->u.swap.hdr_data         = hdr_data;
    entry->u.swap.operand          = operand;
    entry->u.swap.operation        = operation;
    entry->u.swap.datatype         = datatype;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                    sizeof(ptl_cqe_t) );

    return PTL_OK;
}

int PtlAtomicSync(void)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif

    ptl_cqe_t   *entry;
    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLATOMICSYNC;
    entry->u.atomicSync.my_id = get_my_ppe_rank();

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                    sizeof(ptl_cqe_t) );
    return PTL_OK;
}

/* vim:set expandtab */


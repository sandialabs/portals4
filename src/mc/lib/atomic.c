#include "config.h"

#include "portals4.h"
#include "ppe_if.h"

#include "shared/ptl_internal_handles.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_pid.h"

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
    const ptl_internal_handle_converter_t md         = { md_handle };
    
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }   
    if (length > nit_limits[md.s.ni].max_atomic_size) {
        VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[md.s.ni].max_atomic_size);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid MD\n");
        return PTL_ARG_INVALID;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && (length > nit_limits[md.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an MD marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[md.s.ni].max_volatile_size);
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
    switch (md.s.ni) {
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
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for logical/binary operations\n");
                    return PTL_ARG_INVALID;

                default:
                    break;
            }
        default:
            break;
    }
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md.s.ni].max_pt_index);
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
    const ptl_internal_handle_converter_t get_md = { get_md_handle };

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
                      PtlInternalMDLength(get_md_handle), local_get_offset + length);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(put_md_handle) < local_put_offset + length) {
        VERBOSE_ERROR("Swap saw put_md too short for local_offset (%u < %u)\n",
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
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
    }
    switch (operation) {
        case PTL_SWAP:                 
            if (length > nit_limits[get_md.s.ni].max_atomic_size) {
                VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",  
                              (unsigned int)length,
                              (unsigned int)nit_limits[get_md.s.ni].max_atomic_size);
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
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for CSWAP/MSWAP\n");
                    return PTL_ARG_INVALID;    
                default:                    break;
            }         
            break;
        default:
            VERBOSE_ERROR("Only PTL_SWAP/CSWAP/MSWAP may be used with PtlSwap\n"
);  
            return PTL_ARG_INVALID;
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

    return PTL_OK;
}

int PtlAtomicSync(void)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif
    return PTL_OK;
}

/* vim:set expandtab */


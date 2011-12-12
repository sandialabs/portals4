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

static inline int calc_multiple( ptl_datatype_t datatype )
{
    switch (datatype) {
        case PTL_INT8_T:
        case PTL_UINT8_T:
            return 1;
        case PTL_INT16_T:
        case PTL_UINT16_T:
            return 2;
        case PTL_INT32_T:
        case PTL_UINT32_T:
        case PTL_FLOAT:
            return 4;
        case PTL_INT64_T:
        case PTL_UINT64_T:
        case PTL_DOUBLE:
        case PTL_FLOAT_COMPLEX:
            return 8;
        case PTL_LONG_DOUBLE:
        case PTL_DOUBLE_COMPLEX:
            return 16;
        case PTL_LONG_DOUBLE_COMPLEX:
            return 32;
    }
    return 1;
}

#ifndef NO_ARG_VALIDATION
static inline int 
validate_data_movement_args( int type, 
                ptl_handle_md_t md_handle,
                ptl_size_t      local_offset,
                ptl_handle_md_t md_handle2,
                ptl_size_t      local_offset2,
                ptl_process_t   target_id,
                ptl_pt_index_t  pt_index,
                ptl_size_t      remote_offset,
                ptl_handle_ct_t trig_ct_handle
             ) 
{
    const ptl_internal_handle_converter_t md_hc = { md_handle };
    const ptl_internal_handle_converter_t md_hc2 = { md_handle2 };
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

    switch( type ) {
        case PTLFETCHATOMIC:
        case PTLTRIGFETCHATOMIC:
        case PTLSWAP:
        case PTLTRIGSWAP:
            if (md_hc.s.ni != md_hc2.s.ni) {
                VERBOSE_ERROR("MDs *must* be on the same NI\n");
                return PTL_ARG_INVALID;
            }

            if (PtlInternalMDHandleValidator(md_handle2, 1)) {
                VERBOSE_ERROR("%s saw invalid local_offset2\n",__func__);
                return PTL_ARG_INVALID;
            }

            if (local_offset2 >= (1ULL << 48)) {
                VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
                return PTL_ARG_INVALID;
            }           

            break;
    }
    return PTL_OK;
}

static inline int
validate_atomic_args( int type, int ni, ptl_size_t length, 
                ptl_op_t operation, ptl_datatype_t datatype )
{
    if (length > nit_limits[ni].max_atomic_size) {
        VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[ni].max_atomic_size);
        return PTL_ARG_INVALID;
    }
        
    if (length % calc_multiple(datatype) != 0) {
        VERBOSE_ERROR("Length not a multiple of datatype size\n");
        return PTL_ARG_INVALID;
    }

    switch (operation) {
        case PTL_CSWAP:
        case PTL_MSWAP:
        case PTL_SWAP:
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
    return PTL_OK;
}

static inline int
validate_swap_args( int ni, ptl_size_t length, 
                        ptl_op_t operation, ptl_datatype_t datatype )
{
    if (length % calc_multiple(datatype) != 0) {
        VERBOSE_ERROR("Length not a multiple of datatype size\n");
        return PTL_ARG_INVALID;
    }

    switch ( operation ) {
        case PTL_SWAP:            
            if (length > nit_limits[ni].max_atomic_size) {
                VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[ni].max_atomic_size);
                return PTL_ARG_INVALID; 
            }
            break;
        case PTL_CSWAP:
        case PTL_MSWAP:
            if (length > 32) {   
                VERBOSE_ERROR("Length (%u) is bigger than one datatype (32)\n",
                              (unsigned int)length);
                return PTL_ARG_INVALID;            }
            switch (datatype) {  
                case PTL_DOUBLE: 
                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes"
                                                        " for CSWAP/MSWAP\n");
                    return PTL_ARG_INVALID;    
                default: 
                    break;
            }     
            break;
        default:
            VERBOSE_ERROR("Only PTL_SWAP/CSWAP/MSWAP may be used with"
                                                                " PtlSwap\n");
            return PTL_ARG_INVALID;
    }
    return PTL_OK;
}

static inline int 
validate_triggered_args( ptl_handle_ct_t trig_ct_handle )
{
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i);"
                        " max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
    return PTL_OK;
}

#endif  /* ifndef NO_ARG_VALIDATION */

static inline int data_movement_op(
        int type, 
        ptl_handle_md_t  md_handle,
        ptl_size_t       local_offset,
        ptl_handle_md_t  md_handle2,
        ptl_size_t       local_offset2,
        ptl_size_t       length,
        ptl_ack_req_t    ack_req,
        ptl_process_t    target_id,
        ptl_pt_index_t   pt_index,
        ptl_match_bits_t match_bits,
        ptl_size_t       remote_offset,
        void            *user_ptr,
        ptl_hdr_data_t   hdr_data,
        const void      *atomic_operand,
        ptl_op_t         atomic_operation, 
        ptl_datatype_t   atomic_datatype,
        ptl_handle_ct_t  trig_ct_handle,
        ptl_size_t       trig_threshold  
)
{
    const ptl_internal_handle_converter_t md_hc = { md_handle };
    const ptl_internal_handle_converter_t md_hc2 = { md_handle2 };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    ret = validate_data_movement_args( type,
                md_handle, local_offset, 
                md_handle2, local_offset2,
                target_id, pt_index, remote_offset, trig_ct_handle );
    if ( ret != PTL_OK ) return ret;

    switch( type ) {
        case PTLATOMIC:
        case PTLTRIGATOMIC:
        case PTLFETCHATOMIC:
        case PTLTRIGFETCHATOMIC:
            ret = validate_atomic_args( type, md_hc.s.ni, length, atomic_operation, 
                                atomic_datatype );
            if ( ret != PTL_OK ) return ret;
            break; 

        case PTLSWAP:
        case PTLTRIGSWAP:
            ret = validate_swap_args( md_hc.s.ni, length, atomic_operation, 
                                atomic_datatype );
            if ( ret != PTL_OK ) return ret;
            break; 
        }

    switch( type ) {
        case PTLTRIGGET:
        case PTLTRIGPUT:
        case PTLTRIGATOMIC:
        case PTLTRIGFETCHATOMIC:
        case PTLTRIGSWAP:
            ret = validate_triggered_args( trig_ct_handle );
            break;        
    }
#endif

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;
    
    entry->base.type             = type;
    entry->base.remote_id        = ptl_iface_get_rank(&ptl_iface);
    entry->base.ni               = md_hc.s.ni;
    entry->dm.args.md_handle     = md_hc; 
    entry->dm.args.local_offset  = local_offset;
    entry->dm.args.md_handle2    = md_hc2; 
    entry->dm.args.local_offset2 = local_offset2;
    entry->dm.args.length        = length;
    entry->dm.args.ack_req       = ack_req;
    entry->dm.args.target_id     = target_id;
    entry->dm.args.match_bits    = match_bits;
    entry->dm.args.remote_offset = remote_offset;
    entry->dm.args.user_ptr      = user_ptr;
    entry->dm.args.pt_index      = pt_index;
    entry->dm.args.hdr_data      = hdr_data; 

    switch ( type ) {
      case PTLATOMIC:
      case PTLTRIGATOMIC:
      case PTLFETCHATOMIC:
      case PTLTRIGFETCHATOMIC:
      case PTLSWAP:
      case PTLTRIGSWAP:
        
        if ( atomic_operand ) {
            memcpy( &entry->dm.atomic_args.operand, 
                    atomic_operand, calc_multiple(atomic_datatype));      
        }   
        entry->dm.atomic_args.operation = atomic_operation;
        entry->dm.atomic_args.datatype  = atomic_datatype;
        break;
    }

    switch ( type ) {
      case PTLTRIGPUT:
      case PTLTRIGGET:
      case PTLTRIGATOMIC:
      case PTLTRIGFETCHATOMIC:
      case PTLTRIGSWAP:
        entry->dm.triggered_args.ct_handle = 
                            (ptl_internal_handle_converter_t) trig_ct_handle;
        entry->dm.triggered_args.threshold = trig_threshold;
        entry->dm.triggered_args.index = find_triggered_index( md_hc.s.ni ); 
        if ( entry->dm.triggered_args.index == -1 ) {
            ptl_cq_entry_free(ptl_iface_get_cq(&ptl_iface), entry);
            return PTL_FAIL; 
        }
        break;
    }
    
    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface),
                                  entry, sizeof(ptl_cqe_put_t));
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
    return data_movement_op( PTLPUT,
        md_handle, local_offset,
        PTL_INVALID_HANDLE, 0,
        length, ack_req, target_id, pt_index, match_bits, remote_offset,
        user_ptr, hdr_data,
        NULL, 0, 0,
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
    return data_movement_op( PTLTRIGPUT,
        md_handle, local_offset,
        PTL_INVALID_HANDLE, 0,
        length, ack_req, target_id, pt_index, match_bits, remote_offset,
        user_ptr, hdr_data,
        NULL, 0, 0,
        trig_ct_handle, threshold );
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
    return data_movement_op( PTLGET,
        md_handle, local_offset,
        PTL_INVALID_HANDLE, 0, 
        length, PTL_NO_ACK_REQ, target_id, pt_index, match_bits, remote_offset,
        user_ptr, 0,
        NULL, 0, 0,
        PTL_INVALID_HANDLE, 0 );
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
    return data_movement_op( PTLTRIGGET,
        md_handle, local_offset,
        PTL_INVALID_HANDLE, 0,
        length, PTL_NO_ACK_REQ, target_id, pt_index, match_bits, remote_offset,
        user_ptr, 0, 
        NULL, 0, 0,
        trig_ct_handle, trig_threshold );
}

int PtlAtomic(
        ptl_handle_md_t  md_handle,
        ptl_size_t       local_offset,
        ptl_size_t       length,
        ptl_ack_req_t    ack_req,        ptl_process_t    target_id,
        ptl_pt_index_t   pt_index,
        ptl_match_bits_t match_bits,
        ptl_size_t       remote_offset,
        void            *user_ptr,
        ptl_hdr_data_t   hdr_data,
        ptl_op_t         operation,     
        ptl_datatype_t   datatype       
)   
{   
    return data_movement_op( PTLATOMIC,
        md_handle, local_offset,
        PTL_INVALID_HANDLE, 0,
        length, ack_req, target_id, pt_index, match_bits, remote_offset,
        user_ptr, hdr_data,
        NULL, operation, datatype,
        PTL_INVALID_HANDLE, 0 );
}   
    
int PtlTriggeredAtomic(
        ptl_handle_md_t  md_handle,     
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
        ptl_size_t       trig_threshold
)       
{       
    return data_movement_op( PTLTRIGATOMIC,
        md_handle, local_offset,
        PTL_INVALID_HANDLE, 0,
        length, ack_req, target_id, pt_index, match_bits, remote_offset,
        user_ptr, hdr_data,
        NULL, operation, datatype,
        trig_ct_handle, trig_threshold );
}       

int PtlFetchAtomic(
        ptl_handle_md_t  get_md_handle,
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
        ptl_datatype_t   datatype 
)
{
    return data_movement_op( PTLFETCHATOMIC,
        get_md_handle, local_get_offset,
        put_md_handle, local_put_offset,
        length, PTL_NO_ACK_REQ, target_id, pt_index, match_bits, remote_offset, 
        user_ptr, hdr_data, 
        NULL, operation, datatype,
        PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredFetchAtomic(
        ptl_handle_md_t  get_md_handle,
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
        ptl_size_t       trig_threshold
)
{
    return data_movement_op( PTLTRIGFETCHATOMIC,
        get_md_handle, local_get_offset,
        put_md_handle, local_put_offset,
        length, PTL_NO_ACK_REQ, target_id, pt_index, match_bits, remote_offset,
        user_ptr, hdr_data,
        NULL, operation, datatype,
        trig_ct_handle, trig_threshold );
}

int PtlSwap(
        ptl_handle_md_t  get_md_handle,
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
       ptl_datatype_t   datatype
)
{
    return data_movement_op( PTLSWAP,
        get_md_handle, local_get_offset,
        put_md_handle, local_put_offset,
        length, PTL_NO_ACK_REQ, target_id, pt_index, match_bits, remote_offset,
        user_ptr, hdr_data,
        operand, operation, datatype,
        PTL_INVALID_HANDLE, 0 );
}

int PtlTriggeredSwap(
        ptl_handle_md_t  get_md_handle,
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
    return data_movement_op( PTLTRIGSWAP,
        get_md_handle, local_get_offset,
        put_md_handle, local_put_offset,
        length, PTL_NO_ACK_REQ, target_id, pt_index, match_bits, remote_offset,
        user_ptr, hdr_data,
        operand, operation, datatype,
        trig_ct_handle, trig_threshold );
}


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* The API definition */
#include <portals4.h>

/* Internals */
#include "ptl_visibility.h"
#ifndef NO_ARG_VALIDATION
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_DM.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_pid.h"
#include "ptl_internal_commpad.h"
#endif

/*
 * Serialized functions:
 *   CTSet
 * Sometimes serialized functions:
 *   Triggered* - only out-of-order thresholds
 *   CTFree - only if there are triggered ops
 * The progress thread is the only one that can trigger a triggered event
 * Triggered events happen on:
 *   1. message delivery
 *   2. message ack/frame-return
 *   3. serialized functions
 * CT triggered list is a modified nemesis queue to allow safe threshold append
 *   (relies on 128-bit atomics)
 */

int API_FUNC PtlTriggeredPut(
    ptl_handle_md_t md_handle,
    ptl_size_t local_offset,
    ptl_size_t length,
    ptl_ack_req_t ack_req,
    ptl_process_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data,
    ptl_handle_ct_t trig_ct_handle,
    ptl_size_t threshold)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t mdh = { md_handle };
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
	VERBOSE_ERROR("invalid md_handle\n");
	return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(md_handle) < local_offset + length) {
        VERBOSE_ERROR("MD too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(md_handle), local_offset + length);
        return PTL_ARG_INVALID;
    }
    switch (mdh.s.ni) {
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
    if (pt_index > nit_limits[mdh.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n", (unsigned long)pt_index, (unsigned long)nit_limits[mdh.s.ni].max_pt_index);
	return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    //PtlInternalMDPosted(md_handle);
    return PTL_FAIL;
}

int API_FUNC PtlTriggeredGet(
    ptl_handle_md_t md_handle,
    ptl_size_t local_offset,
    ptl_size_t length,
    ptl_process_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    void *user_ptr,
    ptl_size_t remote_offset,
    ptl_handle_ct_t trig_ct_handle,
    ptl_size_t threshold)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t md = { md_handle };
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
	VERBOSE_ERROR("invalid md_handle\n");
	return PTL_ARG_INVALID;
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
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n", (unsigned long)pt_index, (unsigned long)nit_limits[md.s.ni].max_pt_index);
	return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_FAIL;
}

int API_FUNC PtlTriggeredAtomic(
    ptl_handle_md_t md_handle,
    ptl_size_t local_offset,
    ptl_size_t length,
    ptl_ack_req_t ack_req,
    ptl_process_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data,
    ptl_op_t operation,
    ptl_datatype_t datatype,
    ptl_handle_ct_t trig_ct_handle,
    ptl_size_t threshold)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t md = { md_handle };
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
	VERBOSE_ERROR("invalid md_handle\n");
	return PTL_ARG_INVALID;
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_CHAR:
            case PTL_UCHAR:
                multiple = 1;
                break;
            case PTL_SHORT:
            case PTL_USHORT:
                multiple = 2;
                break;
            case PTL_INT:
            case PTL_UINT:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_LONG:
            case PTL_ULONG:
            case PTL_DOUBLE:
                multiple = 8;
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
            VERBOSE_ERROR
                ("SWAP/CSWAP/MSWAP invalid optypes for PtlAtomic()\n");
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
                    VERBOSE_ERROR
                        ("PTL_DOUBLE/PTL_FLOAT invalid datatypes for logical/binary operations\n");
                    return PTL_ARG_INVALID;
                default:
                    break;
            }
        default:
            break;
    }
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n", (unsigned long)pt_index, (unsigned long)nit_limits[md.s.ni].max_pt_index);
	return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_FAIL;
}

int API_FUNC PtlTriggeredFetchAtomic(
    ptl_handle_md_t get_md_handle,
    ptl_size_t local_get_offset,
    ptl_handle_md_t put_md_handle,
    ptl_size_t local_put_offset,
    ptl_size_t length,
    ptl_process_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data,
    ptl_op_t operation,
    ptl_datatype_t datatype,
    ptl_handle_ct_t trig_ct_handle,
    ptl_size_t threshold)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t get_md = { get_md_handle };
    const ptl_internal_handle_converter_t put_md = { put_md_handle };
    if (comm_pad == NULL) {
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
        VERBOSE_ERROR
            ("FetchAtomic saw get_md too short for local_offset (%u < %u)\n",
             PtlInternalMDLength(get_md_handle), local_get_offset + length);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(put_md_handle) < local_put_offset + length) {
        VERBOSE_ERROR
            ("FetchAtomic saw put_md too short for local_offset (%u < %u)\n",
             PtlInternalMDLength(put_md_handle), local_put_offset + length);
        return PTL_ARG_INVALID;
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_CHAR:
            case PTL_UCHAR:
                multiple = 1;
                break;
            case PTL_SHORT:
            case PTL_USHORT:
                multiple = 2;
                break;
            case PTL_INT:
            case PTL_UINT:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_LONG:
            case PTL_ULONG:
            case PTL_DOUBLE:
                multiple = 8;
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
                    VERBOSE_ERROR
                        ("PTL_DOUBLE/PTL_FLOAT invalid datatypes for logical/binary operations\n");
                    return PTL_ARG_INVALID;
                default:
                    break;
            }
        default:
            break;
    }
    if (pt_index > nit_limits[get_md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n", (unsigned long)pt_index, (unsigned long)nit_limits[get_md.s.ni].max_pt_index);
	return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_FAIL;
}

int API_FUNC PtlTriggeredSwap(
    ptl_handle_md_t get_md_handle,
    ptl_size_t local_get_offset,
    ptl_handle_md_t put_md_handle,
    ptl_size_t local_put_offset,
    ptl_size_t length,
    ptl_process_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data,
    void *operand,
    ptl_op_t operation,
    ptl_datatype_t datatype,
    ptl_handle_ct_t trig_ct_handle,
    ptl_size_t threshold)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t get_md = { get_md_handle };
    const ptl_internal_handle_converter_t put_md = { put_md_handle };
    if (comm_pad == NULL) {
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
    if (length > nit_limits[get_md.s.ni].max_atomic_size) {
        VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[get_md.s.ni].max_atomic_size);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(get_md_handle) < local_get_offset + length) {
        VERBOSE_ERROR
            ("Swap saw get_md too short for local_offset (%u < %u)\n",
             PtlInternalMDLength(get_md_handle), local_get_offset + length);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(put_md_handle) < local_put_offset + length) {
        VERBOSE_ERROR
            ("Swap saw put_md too short for local_offset (%u < %u)\n",
             PtlInternalMDLength(put_md_handle), local_put_offset + length);
        return PTL_ARG_INVALID;
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_CHAR:
            case PTL_UCHAR:
                multiple = 1;
                break;
            case PTL_SHORT:
            case PTL_USHORT:
                multiple = 2;
                break;
            case PTL_INT:
            case PTL_UINT:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_LONG:
            case PTL_ULONG:
            case PTL_DOUBLE:
                multiple = 8;
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
            break;
        case PTL_CSWAP:
        case PTL_MSWAP:
            switch (datatype) {
                case PTL_DOUBLE:
                case PTL_FLOAT:
                    VERBOSE_ERROR
                        ("PTL_DOUBLE/PTL_FLOAT invalid datatypes for CSWAP/MSWAP\n");
                    return PTL_ARG_INVALID;
                default:
                    break;
            }
        default:
            VERBOSE_ERROR
                ("Only PTL_SWAP/CSWAP/MSWAP may be used with PtlSwap\n");
            return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[get_md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n", (unsigned long)pt_index, (unsigned long)nit_limits[get_md.s.ni].max_pt_index);
	return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_FAIL;
}

int API_FUNC PtlTriggeredCTInc(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t increment,
    ptl_handle_ct_t trig_ct_handle,
    ptl_size_t threshold)
{
    //const ptl_internal_handle_converter_t ct = { ct_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_FAIL;
}

int API_FUNC PtlTriggeredCTSet(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t new_ct,
    ptl_handle_ct_t trig_ct_handle,
    ptl_size_t threshold)
{
    //const ptl_internal_handle_converter_t ct = { ct_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_FAIL;
}

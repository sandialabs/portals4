#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* The API definition */
#include <portals4.h>

/* System headers */
#include <string.h> /* for memcpy() */
/*#include <stdio.h>
 #include <inttypes.h>*/

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_CT.h"
#ifndef NO_ARG_VALIDATION
# include "ptl_internal_error.h"
# include "ptl_internal_nit.h"
# include "ptl_internal_DM.h"
# include "ptl_internal_pid.h"
# include "ptl_internal_commpad.h"
#endif
#include "ptl_internal_trigger.h"

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

int API_FUNC PtlTriggeredPut(ptl_handle_md_t  md_handle,
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
{   /*{{{*/
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    ptl_internal_trigger_t               *t;

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
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i); max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[mdh.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[mdh.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    {
        ptl_ct_event_t tmp;
        PtlCTGet(trig_ct_handle, &tmp);
        if (tmp.success + tmp.failure >= threshold) {
            return PtlPut(md_handle, local_offset, length, ack_req,
                          target_id, pt_index, match_bits, remote_offset,
                          user_ptr, hdr_data);
        }
    }
    /* 1. Fetch the trigger structure */
    t = PtlInternalFetchTrigger(tct.s.ni);
    /* 2. Build the trigger structure */
    PtlInternalMDPosted(md_handle);
    t->threshold              = threshold;
    t->type                   = PUT;
    t->args.put.md_handle     = md_handle;
    t->args.put.local_offset  = local_offset;
    t->args.put.length        = length;
    t->args.put.ack_req       = ack_req;
    t->args.put.target_id     = target_id;
    t->args.put.pt_index      = pt_index;
    t->args.put.match_bits    = match_bits;
    t->args.put.remote_offset = remote_offset;
    t->args.put.user_ptr      = user_ptr;
    t->args.put.hdr_data      = hdr_data;
    /* append IFF threshold > max_threshold */
    PtlInternalAddTrigger(trig_ct_handle, t);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlTriggeredGet(ptl_handle_md_t  md_handle,
                             ptl_size_t       local_offset,
                             ptl_size_t       length,
                             ptl_process_t    target_id,
                             ptl_pt_index_t   pt_index,
                             ptl_match_bits_t match_bits,
                             ptl_size_t       remote_offset,
                             void            *user_ptr,
                             ptl_handle_ct_t  trig_ct_handle,
                             ptl_size_t       threshold)
{   /*{{{*/
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    ptl_internal_trigger_t               *t;

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
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i); max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    {
        ptl_ct_event_t tmp;
        PtlCTGet(trig_ct_handle, &tmp);
        if (tmp.success + tmp.failure >= threshold) {
            return PtlGet(md_handle, local_offset, length, target_id,
                          pt_index, match_bits, remote_offset,
                          user_ptr);
        }
    }
    /* 1. Fetch the trigger structure */
    t = PtlInternalFetchTrigger(tct.s.ni);
    /* 2. Build the trigger structure */
    PtlInternalMDPosted(md_handle);
    t->threshold              = threshold;
    t->type                   = GET;
    t->args.get.md_handle     = md_handle;
    t->args.get.local_offset  = local_offset;
    t->args.get.length        = length;
    t->args.get.target_id     = target_id;
    t->args.get.pt_index      = pt_index;
    t->args.get.match_bits    = match_bits;
    t->args.get.remote_offset = remote_offset;
    t->args.get.user_ptr      = user_ptr;
    /* append IFF threshold > max_threshold */
    PtlInternalAddTrigger(trig_ct_handle, t);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlTriggeredAtomic(ptl_handle_md_t  md_handle,
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
                                ptl_size_t       threshold)
{   /*{{{*/
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    ptl_internal_trigger_t               *t;

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
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i); max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    {
        ptl_ct_event_t tmp;
        PtlCTGet(trig_ct_handle, &tmp);
        if (tmp.success + tmp.failure >= threshold) {
            return PtlAtomic(md_handle, local_offset, length, ack_req,
                             target_id, pt_index, match_bits, remote_offset,
                             user_ptr, hdr_data, operation, datatype);
        }
    }
    /* 1. Fetch the trigger structure */
    t = PtlInternalFetchTrigger(tct.s.ni);
    /* 2. Build the trigger structure */
    PtlInternalMDPosted(md_handle);
    t->threshold                 = threshold;
    t->type                      = ATOMIC;
    t->args.atomic.md_handle     = md_handle;
    t->args.atomic.local_offset  = local_offset;
    t->args.atomic.length        = length;
    t->args.atomic.ack_req       = ack_req;
    t->args.atomic.target_id     = target_id;
    t->args.atomic.pt_index      = pt_index;
    t->args.atomic.match_bits    = match_bits;
    t->args.atomic.remote_offset = remote_offset;
    t->args.atomic.user_ptr      = user_ptr;
    t->args.atomic.hdr_data      = hdr_data;
    t->args.atomic.operation     = operation;
    t->args.atomic.datatype      = datatype;
    /* append IFF threshold > max_threshold */
    PtlInternalAddTrigger(trig_ct_handle, t);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlTriggeredFetchAtomic(ptl_handle_md_t  get_md_handle,
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
                                     ptl_size_t       threshold)
{   /*{{{*/
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    ptl_internal_trigger_t               *t;

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
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i); max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[get_md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[get_md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    return PTL_FAIL;

    {
        ptl_ct_event_t tmp;
        PtlCTGet(trig_ct_handle, &tmp);
        if (tmp.success + tmp.failure >= threshold) {
            return PtlFetchAtomic(get_md_handle, local_get_offset,
                                  put_md_handle, local_put_offset,
                                  length, target_id, pt_index, match_bits,
                                  remote_offset, user_ptr, hdr_data,
                                  operation, datatype);
        }
    }
    /* 1. Fetch the trigger structure */
    t = PtlInternalFetchTrigger(tct.s.ni);
    /* 2. Build the trigger structure */
    PtlInternalMDPosted(get_md_handle);
    PtlInternalMDPosted(put_md_handle);
    t->threshold                         = threshold;
    t->type                              = FETCHATOMIC;
    t->args.fetchatomic.get_md_handle    = get_md_handle;
    t->args.fetchatomic.local_get_offset = local_get_offset;
    t->args.fetchatomic.put_md_handle    = put_md_handle;
    t->args.fetchatomic.local_put_offset = local_put_offset;
    t->args.fetchatomic.length           = length;
    t->args.fetchatomic.target_id        = target_id;
    t->args.fetchatomic.pt_index         = pt_index;
    t->args.fetchatomic.match_bits       = match_bits;
    t->args.fetchatomic.remote_offset    = remote_offset;
    t->args.fetchatomic.user_ptr         = user_ptr;
    t->args.fetchatomic.hdr_data         = hdr_data;
    t->args.fetchatomic.operation        = operation;
    t->args.fetchatomic.datatype         = datatype;
    /* append IFF threshold > max_threshold */
    PtlInternalAddTrigger(trig_ct_handle, t);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlTriggeredSwap(ptl_handle_md_t  get_md_handle,
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
                              void            *operand,
                              ptl_op_t         operation,
                              ptl_datatype_t   datatype,
                              ptl_handle_ct_t  trig_ct_handle,
                              ptl_size_t       threshold)
{   /*{{{*/
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    ptl_internal_trigger_t               *t;

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
            break;
        case PTL_CSWAP:
        case PTL_MSWAP:
            switch (datatype) {
                case PTL_DOUBLE:
                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for CSWAP/MSWAP\n");
                    return PTL_ARG_INVALID;

                default:
                    break;
            }
        default:
            VERBOSE_ERROR("Only PTL_SWAP/CSWAP/MSWAP may be used with PtlSwap\n");
            return PTL_ARG_INVALID;
    }
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i); max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[get_md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[get_md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(trig_ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    return PTL_FAIL;

    {
        ptl_ct_event_t tmp;
        PtlCTGet(trig_ct_handle, &tmp);
        if (tmp.success + tmp.failure >= threshold) {
            return PtlSwap(get_md_handle, local_get_offset,
                           put_md_handle, local_put_offset,
                           length, target_id, pt_index, match_bits,
                           remote_offset, user_ptr, hdr_data,
                           operand, operation, datatype);
        }
    }
    /* 1. Fetch the trigger structure */
    t = PtlInternalFetchTrigger(tct.s.ni);
    /* 2. Build the trigger structure */
    PtlInternalMDPosted(get_md_handle);
    PtlInternalMDPosted(put_md_handle);
    t->threshold                  = threshold;
    t->type                       = SWAP;
    t->args.swap.get_md_handle    = get_md_handle;
    t->args.swap.local_get_offset = local_get_offset;
    t->args.swap.put_md_handle    = put_md_handle;
    t->args.swap.local_put_offset = local_put_offset;
    t->args.swap.length           = length;
    t->args.swap.target_id        = target_id;
    t->args.swap.pt_index         = pt_index;
    t->args.swap.match_bits       = match_bits;
    t->args.swap.remote_offset    = remote_offset;
    t->args.swap.user_ptr         = user_ptr;
    t->args.swap.hdr_data         = hdr_data;
    if ((operation == PTL_CSWAP) || (operation == PTL_MSWAP)) {
        switch (datatype) {
            case PTL_CHAR:
            case PTL_UCHAR:
                memcpy(t->args.swap.operand, operand, 1);
                break;
            case PTL_SHORT:
            case PTL_USHORT:
                memcpy(t->args.swap.operand, operand, 2);
                break;
            case PTL_INT:
            case PTL_UINT:
            case PTL_FLOAT:
                memcpy(t->args.swap.operand, operand, 4);
                break;
            case PTL_LONG:
            case PTL_ULONG:
            case PTL_DOUBLE:
            case PTL_FLOAT_COMPLEX:
                memcpy(t->args.swap.operand, operand, 8);
                break;
            case PTL_LONG_DOUBLE:
            case PTL_DOUBLE_COMPLEX:
                memcpy(t->args.swap.operand, operand, 16);
                break;
            case PTL_LONG_DOUBLE_COMPLEX:
                memcpy(t->args.swap.operand, operand, 32);
                break;
        }
    }
    t->args.swap.operation = operation;
    t->args.swap.datatype  = datatype;
    /* append IFF threshold > max_threshold */
    PtlInternalAddTrigger(trig_ct_handle, t);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlTriggeredCTInc(ptl_handle_ct_t ct_handle,
                               ptl_ct_event_t  increment,
                               ptl_handle_ct_t trig_ct_handle,
                               ptl_size_t      threshold)
{   /*{{{*/
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    ptl_internal_trigger_t               *t;

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
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i); max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
    if ((increment.success != 0) && (increment.failure != 0)) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    {
        ptl_ct_event_t tmp;
        PtlCTGet(trig_ct_handle, &tmp);
        if (tmp.success + tmp.failure >= threshold) {
            return PtlCTInc(ct_handle, increment);
        }
    }
    /* 1. Fetch the trigger structure */
    t = PtlInternalFetchTrigger(tct.s.ni);
    /* 2. Build the trigger structure */
    t->threshold            = threshold;
    t->type                 = CTINC;
    t->args.ctinc.ct_handle = ct_handle;
    t->args.ctinc.increment = increment;
    /* append IFF threshold > max_threshold */
    PtlInternalAddTrigger(trig_ct_handle, t);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlTriggeredCTSet(ptl_handle_ct_t ct_handle,
                               ptl_ct_event_t  new_ct,
                               ptl_handle_ct_t trig_ct_handle,
                               ptl_size_t      threshold)
{   /*{{{*/
    const ptl_internal_handle_converter_t tct = { trig_ct_handle };
    ptl_internal_trigger_t               *t;

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
    if (nit_limits[tct.s.ni].max_triggered_ops == 0) {
        VERBOSE_ERROR("Triggered operations not allowed on this NI (%i); max_triggered_ops set to zero\n", tct.s.ni);
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    {
        ptl_ct_event_t tmp;
        PtlCTGet(trig_ct_handle, &tmp);
        if (tmp.success + tmp.failure >= threshold) {
            return PtlCTSet(ct_handle, new_ct);
        }
    }
    /* 1. Fetch the trigger structure */
    t = PtlInternalFetchTrigger(tct.s.ni);
    /* 2. Build the trigger structure */
    t->threshold            = threshold;
    t->type                 = CTSET;
    t->args.ctset.ct_handle = ct_handle;
    t->args.ctset.newval    = new_ct;
    /* append IFF threshold > max_threshold */
    PtlInternalAddTrigger(trig_ct_handle, t);
    return PTL_OK;
} /*}}}*/

void INTERNAL PtlInternalTriggerPull(ptl_internal_trigger_t *t)
{   /*{{{*/
    switch(t->type) {
        case PUT:
            PtlPut(t->args.put.md_handle,
                   t->args.put.local_offset,
                   t->args.put.length,
                   t->args.put.ack_req,
                   t->args.put.target_id,
                   t->args.put.pt_index,
                   t->args.put.match_bits,
                   t->args.put.remote_offset,
                   t->args.put.user_ptr,
                   t->args.put.hdr_data);
            PtlInternalMDCleared(t->args.put.md_handle);
            break;
        case GET:
            PtlGet(t->args.get.md_handle,
                   t->args.get.local_offset,
                   t->args.get.length,
                   t->args.get.target_id,
                   t->args.get.pt_index,
                   t->args.get.match_bits,
                   t->args.get.remote_offset,
                   t->args.get.user_ptr);
            PtlInternalMDCleared(t->args.get.md_handle);
            break;
        case ATOMIC:
            PtlAtomic(t->args.atomic.md_handle,
                      t->args.atomic.local_offset,
                      t->args.atomic.length,
                      t->args.atomic.ack_req,
                      t->args.atomic.target_id,
                      t->args.atomic.pt_index,
                      t->args.atomic.match_bits,
                      t->args.atomic.remote_offset,
                      t->args.atomic.user_ptr,
                      t->args.atomic.hdr_data,
                      t->args.atomic.operation,
                      t->args.atomic.datatype);
            PtlInternalMDCleared(t->args.atomic.md_handle);
            break;
        case FETCHATOMIC:
            PtlFetchAtomic(t->args.fetchatomic.get_md_handle,
                           t->args.fetchatomic.local_get_offset,
                           t->args.fetchatomic.put_md_handle,
                           t->args.fetchatomic.local_put_offset,
                           t->args.fetchatomic.length,
                           t->args.fetchatomic.target_id,
                           t->args.fetchatomic.pt_index,
                           t->args.fetchatomic.match_bits,
                           t->args.fetchatomic.remote_offset,
                           t->args.fetchatomic.user_ptr,
                           t->args.fetchatomic.hdr_data,
                           t->args.fetchatomic.operation,
                           t->args.fetchatomic.datatype);
            PtlInternalMDCleared(t->args.fetchatomic.get_md_handle);
            PtlInternalMDCleared(t->args.fetchatomic.put_md_handle);
            break;
        case SWAP:
            PtlSwap(t->args.swap.get_md_handle,
                    t->args.swap.local_get_offset,
                    t->args.swap.put_md_handle,
                    t->args.swap.local_put_offset,
                    t->args.swap.length,
                    t->args.swap.target_id,
                    t->args.swap.pt_index,
                    t->args.swap.match_bits,
                    t->args.swap.remote_offset,
                    t->args.swap.user_ptr,
                    t->args.swap.hdr_data,
                    t->args.swap.operand,
                    t->args.swap.operation,
                    t->args.swap.datatype);
            PtlInternalMDCleared(t->args.swap.get_md_handle);
            PtlInternalMDCleared(t->args.swap.put_md_handle);
            break;
        case CTINC:
            PtlCTInc(t->args.ctinc.ct_handle, t->args.ctinc.increment);
            break;
        case CTSET:
            PtlCTSet(t->args.ctset.ct_handle, t->args.ctset.newval);
            break;
    }
} /*}}}*/

/* vim:set expandtab: */

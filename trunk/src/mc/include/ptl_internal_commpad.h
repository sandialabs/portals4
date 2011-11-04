#ifndef PTL_INTERNAL_COMMPAD_H
#define PTL_INTERNAL_COMMPAD_H

#include "config.h"

#include <portals4.h>

extern size_t    num_siblings;
extern ptl_pid_t proc_number;


typedef struct {
    void            *next;
    ptl_match_bits_t match_bits;
    void            *user_ptr;
    ptl_hdr_data_t   hdr_data;                // not used by GETs
#ifdef STRICT_UID_JID
    ptl_jid_t        jid;
#endif
    /* data used for long & truncated messages */
    void    *entry;
    uint32_t remaining;
#ifdef USE_TRANSFER_ENGINE
    uint64_t xfe_handle1;                        // always used for XFE transfers
    uint64_t xfe_handle2;                        // only used for FetchAtomic & Swap
#else
    uint8_t *moredata;
#endif
    /* data used for GETs and properly processing events */
    ptl_internal_handle_converter_t md_handle1;
    ptl_internal_handle_converter_t md_handle2;
    uint32_t                        length;
    uint16_t                        src;
    uint16_t                        target;
    uint64_t                        local_offset1    : 48;
    uint64_t                        local_offset2    : 48;
    uint64_t                        dest_offset      : 48;
    uint8_t                         pt_index         : 6; // only need 5
    ptl_ack_req_t                   ack_req          : 2; // only used by PUTs and ATOMICs
    unsigned char                   type             : 5; // 0=put, 1=get, 2=atomic, 3=fetchatomic, 4=swap
    unsigned char                   ni               : 2;
    uint8_t                         atomic_operation : 5;
    uint8_t                         atomic_datatype  : 4;
    uint8_t                         data[];
} ptl_internal_header_t;

extern size_t LARGE_FRAG_PAYLOAD;

static inline int PtlInternalLibraryInitialized(void)
{
    return 1;
}

#endif

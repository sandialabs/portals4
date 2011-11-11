#ifndef PTL_INTERNAL_COMMPAD_H
#define PTL_INTERNAL_COMMPAD_H

#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>                    /* for size_t */
#include <sys/types.h>                 /* for pid_t, according to P90 */

#include "ptl_internal_nemesis.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_transfer_engine.h"

extern size_t    num_siblings;
extern ptl_pid_t proc_number;
extern size_t    per_proc_comm_buf_size;
extern size_t    firstpagesize;

enum hdr_types {
    HDR_TYPE_PUT         = 0,      /* _____ */
    HDR_TYPE_GET         = 1,      /* ____1 */
    HDR_TYPE_ATOMIC      = 2,      /* ___1_ */
    HDR_TYPE_FETCHATOMIC = 3,      /* ___11 */
    HDR_TYPE_SWAP        = 4,      /* __1__ */
    HDR_TYPE_CMD         = 5,      /* __1_1 */
    HDR_TYPE_ACKFLAG     = 8,      /* _1___ */
    HDR_TYPE_ACKMASK     = 23,     /* 1_111 */
    HDR_TYPE_TRUNCFLAG   = 16,     /* 1____ */
    HDR_TYPE_BASICMASK   = 7,      /* __111 */
    HDR_TYPE_TERM        = 31      /* 11111 */
};

enum cmd_types {
    CMD_TYPE_CTFREE,
    CMD_TYPE_CHECK,
    CMD_TYPE_ENQUEUE,
    CMD_TYPE_CANCEL,
    CMD_TYPE_NOOP
};

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

typedef struct {
    ptl_internal_header_t hdr;
    size_t                buffered_size;
    void                 *unexpected_entry;
    void                 *buffered_data;
} ptl_internal_buffered_header_t;

struct rank_comm_pad {
    NEMESIS_blocking_queue receiveQ;
    uint64_t               owner;
    uint8_t                data[];
};

extern struct rank_comm_pad *comm_pads[PTL_PID_MAX];

int INTERNAL PtlInternalLibraryInitialized(void);

#endif /* ifndef PTL_INTERNAL_COMMPAD_H */
/* vim:set expandtab: */

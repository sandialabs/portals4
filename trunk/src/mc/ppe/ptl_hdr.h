
#ifndef MC_PTL_HDR_T 
#define MC_PTL_HDR_T 

#include <portals4.h>

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

typedef union {
    ptl_rank_t rank;
    ptl_pid_t pid;
} ptl_hdr_app_id_t;

typedef struct {
    void *next;
    ptl_match_bits_t match_bits;
    ptl_hdr_data_t   hdr_data;                // not used by GETs
    ptl_hdr_app_id_t src;
    ptl_hdr_app_id_t target;
    // how many bits is a md index?
    uint16_t         md_index;

    void           *entry;
    
    uint32_t        remaining;
    ptl_size_t      length;

    uint64_t                        dest_offset      : 48;
    uint8_t                         pt_index         : 6; // only need 5
    ptl_ack_req_t                   ack_req          : 2; // only used by PUTs and ATOMICs
    unsigned char                   type             : 5; // 0=put, 1=get, 2=atomic, 3=fetchatomic, 4=swap
    unsigned char                   ni               : 2;
    uint8_t                         atomic_operation : 5;
    uint8_t                         atomic_datatype  : 4;
    uint8_t                         data[];
} ptl_internal_header_t;

typedef ptl_internal_header_t ptl_hdr_t;

typedef struct {
    ptl_internal_header_t hdr;
    size_t                buffered_size;
    void                 *unexpected_entry;
    void                 *buffered_data;
} ptl_internal_buffered_header_t;


#endif

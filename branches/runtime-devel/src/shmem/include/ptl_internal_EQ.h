#ifndef PTL_INTERNAL_EQ_H
#define PTL_INTERNAL_EQ_H

#include "ptl_internal_alignment.h"

typedef uint16_t ptl_internal_uid_t;

typedef struct {
    ptl_match_bits_t match_bits; // 8 bytes
    void            *start;      // 8 bytes (16)
    void            *user_ptr;   // 8 bytes (24)
    ptl_hdr_data_t   hdr_data;   // 8 bytes (32)
    uint32_t         rlength;    // 4 bytes (36)
    uint32_t         mlength;    // 4 bytes (40)
    union {
        uint32_t rank;
        struct {
            uint16_t nid;
            uint16_t pid;
        } phys;
    } initiator;                              // 4 bytes (44)
    ptl_internal_uid_t uid;                   // 2 bytes (46)
    uint8_t            type;                  // 1 byte  (47)
    uint64_t           remote_offset    : 48; // 6 bytes (53)
    uint8_t            pt_index         : 5;
    uint8_t            atomic_operation : 5;
    uint8_t            ni_fail_type     : 3;
    uint8_t            atomic_type      : 4; // 2-ish bytes (55)
} ptl_internal_event_t ALIGNED (CACHELINE_WIDTH);

#ifndef NO_ARG_VALIDATION
int PtlInternalEQHandleValidator(ptl_handle_eq_t handle,
                                 int             none_ok);
#endif
void PtlInternalEQPush(ptl_handle_eq_t       handle,
                       ptl_internal_event_t *event);
void PtlInternalEQPushESEND(const ptl_handle_eq_t eq_handle,
                            const uint32_t        length,
                            const uint64_t        roffset,
                            void *const           user_ptr);
void PtlInternalEQNISetup(unsigned int ni);
void PtlInternalEQNITeardown(unsigned int ni);

#endif /* ifndef PTL_INTERNAL_EQ_H */
/* vim:set expandtab: */

#ifndef MC_PPE_H
#define MC_PPE_H

#include <stdio.h>
#define PPE_DBG( fmt, args...) \
fprintf(stderr,"%s():%i: " fmt, __FUNCTION__, __LINE__, ## args);

#include "shared/ptl_command_queue.h"
#include "shared/ptl_command_queue_entry.h"

struct ptl_ppe_t {
    ptl_cq_handle_t cq_h;
};

typedef struct ptl_ppe_t ptl_ppe_t;

int ni_init_impl( ptl_ppe_t *ctx, cmdPtlNIInit_t *cmd );
int ni_fini_impl( ptl_ppe_t *ctx, cmdPtlNIFini_t *cmd );
int ct_alloc_impl( ptl_ppe_t *ctx, cmdPtlCTAlloc_t *cmd );
int ct_free_impl( ptl_ppe_t *ctx, cmdPtlCTFree_t *cmd );
int ct_set_impl( ptl_ppe_t *ctx, cmdPtlCTSet_t *cmd );
int ct_inc_impl( ptl_ppe_t *ctx, cmdPtlCTInc_t *cmd );
int atomic_impl( ptl_ppe_t *ctx, cmdPtlAtomic_t *cmd );
int fetch_atomic_impl( ptl_ppe_t *ctx, cmdPtlFetchAtomic_t *cmd );
int swap_impl( ptl_ppe_t *ctx, cmdPtlSwap_t *cmd );
int atomic_sync_impl( ptl_ppe_t *ctx, cmdPtlAtomicSync_t *cmd );
int eq_alloc_impl( ptl_ppe_t *ctx, cmdPtlEQAlloc_t *cmd );
int eq_free_impl( ptl_ppe_t *ctx, cmdPtlEQFree_t *cmd );
int md_bind_impl( ptl_ppe_t *ctx, cmdPtlMDBind_t *cmd );
int md_release_impl( ptl_ppe_t *ctx, cmdPtlMDRelease_t *cmd );
int put_impl( ptl_ppe_t *ctx, cmdPtlPut_t *cmd );
int get_impl( ptl_ppe_t *ctx, cmdPtlGet_t *cmd );
int pt_alloc_impl( ptl_ppe_t *ctx, cmdPtlPTAlloc_t *cmd );
int pt_free_impl( ptl_ppe_t *ctx, cmdPtlPTFree_t *cmd );
int me_append_impl( ptl_ppe_t *ctx, cmdPtlMEAppend_t *cmd );
int me_unlink_impl( ptl_ppe_t *ctx, cmdPtlMEUnlink_t *cmd );
int me_search_impl( ptl_ppe_t *ctx, cmdPtlMESearch_t *cmd );
int le_append_impl( ptl_ppe_t *ctx, cmdPtlLEAppend_t *cmd );
int le_unlink_impl( ptl_ppe_t *ctx, cmdPtlLEUnlink_t *cmd );
int le_search_impl( ptl_ppe_t *ctx, cmdPtlLESearch_t *cmd );
int atomic_impl( ptl_ppe_t *ctx, cmdPtlAtomic_t *cmd );
int fetch_atomic_impl( ptl_ppe_t *ctx, cmdPtlFetchAtomic_t *cmd );
int swap_impl( ptl_ppe_t *ctx, cmdPtlSwap_t *cmd );
int atomic_sync_impl( ptl_ppe_t *ctx, cmdPtlAtomicSync_t *cmd );

#endif

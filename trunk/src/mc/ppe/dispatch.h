#ifndef MC_PPE_DISPATCH_H
#define MC_PPE_DISPATCH_H

struct ptl_ppe_t;

#include "shared/ptl_command_queue_entry.h"

int proc_attach_impl(ptl_ppe_t *ptl_ppe, ptl_cqe_proc_attach_t *attach);
int ni_init_impl(struct ptl_ppe_t *ctx, ptl_cqe_niinit_t *cmd );
int ni_fini_impl(struct ptl_ppe_t *ctx, ptl_cqe_nifini_t *cmd );
int setmap_impl(struct ptl_ppe_t *ctx, ptl_cqe_setmap_t *cmd);
int getmap_impl(struct ptl_ppe_t *ctx, ptl_cqe_getmap_t *cmd);
int ct_alloc_impl(struct ptl_ppe_ni_t *, ptl_cqe_ctalloc_t * );
int ct_free_impl(struct ptl_ppe_ni_t *, ptl_cqe_ctfree_t * );
int ct_op_impl(struct ptl_ppe_ni_t *, int type, ptl_ctop_args_t * );
int triggered_impl(struct ptl_ppe_ni_t *, int type,
                                        ptl_triggered_args_t *, void *op);
int atomic_sync_impl(struct ptl_ppe_t *ctx, ptl_cqe_atomicsync_t *cmd );
int eq_alloc_impl(struct ptl_ppe_t *ctx, ptl_cqe_eqalloc_t *cmd );
int eq_free_impl(struct ptl_ppe_t *ctx, ptl_cqe_eqfree_t *cmd );
int md_bind_impl(struct ptl_ppe_t *ctx, ptl_cqe_mdbind_t *cmd );
int md_release_impl(struct ptl_ppe_t *ctx, ptl_cqe_mdrelease_t *cmd );
int data_movement_impl(struct ptl_ppe_t *ctx, ptl_cqe_data_movement_t *cmd );
int pt_alloc_impl(struct ptl_ppe_t *ctx, ptl_cqe_ptalloc_t *cmd );
int pt_free_impl(struct ptl_ppe_t *ctx, ptl_cqe_ptfree_t *cmd );
int me_append_impl(struct ptl_ppe_t *ctx, ptl_cqe_meappend_t *cmd );
int me_unlink_impl(struct ptl_ppe_t *ctx, ptl_cqe_meunlink_t *cmd );
int me_search_impl(struct ptl_ppe_t *ctx, ptl_cqe_mesearch_t *cmd );
int le_append_impl(struct ptl_ppe_t *ctx, ptl_cqe_leappend_t *cmd );
int le_unlink_impl(struct ptl_ppe_t *ctx, ptl_cqe_leunlink_t *cmd );
int le_search_impl(struct ptl_ppe_t *ctx, ptl_cqe_lesearch_t *cmd );

#endif

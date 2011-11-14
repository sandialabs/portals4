#ifndef MC_PPE_DISPATCH_H
#define MC_PPE_DISPATCH_H

struct ptl_ppe_t;

#include "shared/ptl_command_queue_entry.h"

int proc_attach_impl(ptl_ppe_t *ptl_ppe, ptl_cqe_proc_attach_t *attach);
int ni_init_impl(struct ptl_ppe_t *ctx, ptl_cqe_niinit_t *cmd );
int ni_fini_impl(struct ptl_ppe_t *ctx, ptl_cqe_nifini_t *cmd );
int ct_alloc_impl(struct ptl_ppe_t *ctx, ptl_cqe_ctalloc_t *cmd );
int ct_free_impl(struct ptl_ppe_t *ctx, ptl_cqe_ctfree_t *cmd );
int ct_set_impl(struct ptl_ppe_t *ctx, ptl_cqe_ctset_t *cmd );
int ct_inc_impl(struct ptl_ppe_t *ctx, ptl_cqe_ctinc_t *cmd );
int atomic_impl(struct ptl_ppe_t *ctx, ptl_cqe_atomic_t *cmd );
int fetch_atomic_impl(struct ptl_ppe_t *ctx, ptl_cqe_fetchatomic_t *cmd );
int swap_impl(struct ptl_ppe_t *ctx, ptl_cqe_swap_t *cmd );
int atomic_sync_impl(struct ptl_ppe_t *ctx, ptl_cqe_atomicsync_t *cmd );
int eq_alloc_impl(struct ptl_ppe_t *ctx, ptl_cqe_eqalloc_t *cmd );
int eq_free_impl(struct ptl_ppe_t *ctx, ptl_cqe_eqfree_t *cmd );
int md_bind_impl(struct ptl_ppe_t *ctx, ptl_cqe_mdbind_t *cmd );
int md_release_impl(struct ptl_ppe_t *ctx, ptl_cqe_mdrelease_t *cmd );
int put_impl(struct ptl_ppe_t *ctx, ptl_cqe_put_t *cmd );
int get_impl(struct ptl_ppe_t *ctx, ptl_cqe_get_t *cmd );
int pt_alloc_impl(struct ptl_ppe_t *ctx, ptl_cqe_ptalloc_t *cmd );
int pt_free_impl(struct ptl_ppe_t *ctx, ptl_cqe_ptfree_t *cmd );
int me_append_impl(struct ptl_ppe_t *ctx, ptl_cqe_meappend_t *cmd );
int me_unlink_impl(struct ptl_ppe_t *ctx, ptl_cqe_meunlink_t *cmd );
int me_search_impl(struct ptl_ppe_t *ctx, ptl_cqe_mesearch_t *cmd );
int le_append_impl(struct ptl_ppe_t *ctx, ptl_cqe_leappend_t *cmd );
int le_unlink_impl(struct ptl_ppe_t *ctx, ptl_cqe_leunlink_t *cmd );
int le_search_impl(struct ptl_ppe_t *ctx, ptl_cqe_lesearch_t *cmd );
int atomic_impl(struct ptl_ppe_t *ctx, ptl_cqe_atomic_t *cmd );
int fetch_atomic_impl(struct ptl_ppe_t *ctx, ptl_cqe_fetchatomic_t *cmd );
int swap_impl(struct ptl_ppe_t *ctx, ptl_cqe_swap_t *cmd );
int atomic_sync_impl(struct ptl_ppe_t *ctx, ptl_cqe_atomicsync_t *cmd );

#endif

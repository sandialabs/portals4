/*
 * api.h - API wrappers
 */

#ifndef API_H
#define API_H

struct node_info;

int test_ptl_init(struct node_info *info);

int test_ptl_fini(struct node_info *info);

int test_ptl_ni_init(struct node_info *info);

int test_ptl_ni_fini(struct node_info *info);

int test_ptl_ni_status(struct node_info *info);

int test_ptl_ni_handle(struct node_info *info);

int test_ptl_handle_is_eq(struct node_info *info);

int test_ptl_get_uid(struct node_info *info);

int test_ptl_get_id(struct node_info *info);

int test_ptl_get_jid(struct node_info *info);

int test_ptl_pt_alloc(struct node_info *info);

int test_ptl_pt_free(struct node_info *info);

int test_ptl_pt_disable(struct node_info *info);

int test_ptl_pt_enable(struct node_info *info);

int test_ptl_eq_alloc(struct node_info *info);

int test_ptl_eq_free(struct node_info *info);

int test_ptl_eq_get(struct node_info *info);

int test_ptl_eq_wait(struct node_info *info);

int test_ptl_eq_poll(struct node_info *info);

int test_ptl_ct_alloc(struct node_info *info);

int test_ptl_ct_free(struct node_info *info);

int test_ptl_ct_get(struct node_info *info);

int test_ptl_ct_wait(struct node_info *info);

int test_ptl_ct_poll(struct node_info *info);

int test_ptl_ct_set(struct node_info *info);

int test_ptl_ct_inc(struct node_info *info);

int test_ptl_md_bind(struct node_info *info);

int test_ptl_md_release(struct node_info *info);

int test_ptl_le_append(struct node_info *info);

int test_ptl_le_unlink(struct node_info *info);

int test_ptl_le_search(struct node_info *info);

int test_ptl_me_append(struct node_info *info);

int test_ptl_me_unlink(struct node_info *info);

int test_ptl_me_search(struct node_info *info);

int test_ptl_put(struct node_info *info);

int test_ptl_get(struct node_info *info);

int test_ptl_atomic(struct node_info *info);

int test_ptl_fetch_atomic(struct node_info *info);

int test_ptl_swap(struct node_info *info);

int test_ptl_trig_put(struct node_info *info);

int test_ptl_trig_get(struct node_info *info);

int test_ptl_trig_atomic(struct node_info *info);

int test_ptl_trig_fetch_atomic(struct node_info *info);

int test_ptl_trig_swap(struct node_info *info);

int test_ptl_trig_ct_inc(struct node_info *info);

int test_ptl_trig_ct_set(struct node_info *info);

int test_ptl_start_bundle(struct node_info *info);

int test_ptl_end_bundle(struct node_info *info);

/* RUNTIME APIs */
int test_ptl_set_jid(struct node_info *info);

#endif /* API_H */

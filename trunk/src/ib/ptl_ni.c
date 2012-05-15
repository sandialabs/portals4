/*
 * ptl_ni.c
 */

#include "ptl_loc.h"

static void set_limits(ni_t *ni, const ptl_ni_limits_t *desired)
{
	if (desired) {
		ni->limits.max_entries =
			chk_param(PTL_LIM_MAX_ENTRIES,
				  desired->max_entries);
		ni->limits.max_unexpected_headers =
			chk_param(PTL_LIM_MAX_UNEXPECTED_HEADERS,
				  desired->max_unexpected_headers);
		ni->limits.max_mds =
			chk_param(PTL_LIM_MAX_MDS, desired->max_mds);
		ni->limits.max_cts =
			chk_param(PTL_LIM_MAX_CTS, desired->max_cts);
		ni->limits.max_eqs =
			chk_param(PTL_LIM_MAX_EQS, desired->max_eqs);
		ni->limits.max_pt_index =
			chk_param(PTL_LIM_MAX_PT_INDEX,
				  desired->max_pt_index);
		ni->limits.max_iovecs =
			chk_param(PTL_LIM_MAX_IOVECS,
				  desired->max_iovecs);
		ni->limits.max_list_size =
			chk_param(PTL_LIM_MAX_LIST_SIZE,
				  desired->max_list_size);
		ni->limits.max_triggered_ops =
			chk_param(PTL_LIM_MAX_TRIGGERED_OPS,
				  desired->max_triggered_ops);
		ni->limits.max_msg_size =
			chk_param(PTL_LIM_MAX_MSG_SIZE,
				  desired->max_msg_size);
		ni->limits.max_atomic_size =
			chk_param(PTL_LIM_MAX_ATOMIC_SIZE,
				  desired->max_atomic_size);
		ni->limits.max_fetch_atomic_size =
			chk_param(PTL_LIM_MAX_FETCH_ATOMIC_SIZE,
				  desired->max_fetch_atomic_size);
		ni->limits.max_waw_ordered_size =
			chk_param(PTL_LIM_MAX_WAW_ORDERED_SIZE,
				  desired->max_waw_ordered_size);
		ni->limits.max_war_ordered_size =
			chk_param(PTL_LIM_MAX_WAR_ORDERED_SIZE,
				  desired->max_war_ordered_size);
		ni->limits.max_volatile_size =
			chk_param(PTL_LIM_MAX_VOLATILE_SIZE,
				  desired->max_volatile_size);
		ni->limits.features =
			chk_param(PTL_LIM_FEATURES,
				  desired->features);
		if (desired->features & PTL_TARGET_BIND_INACCESSIBLE)
			ni->limits.features |= PTL_TARGET_BIND_INACCESSIBLE;
	} else {
		ni->limits.max_entries =
			get_param(PTL_LIM_MAX_ENTRIES);
		ni->limits.max_unexpected_headers =
			get_param(PTL_LIM_MAX_UNEXPECTED_HEADERS);
		ni->limits.max_mds =
			get_param(PTL_LIM_MAX_MDS);
		ni->limits.max_cts =
			get_param(PTL_LIM_MAX_CTS);
		ni->limits.max_eqs =
			get_param(PTL_LIM_MAX_EQS);
		ni->limits.max_pt_index =
			get_param(PTL_LIM_MAX_PT_INDEX);
		ni->limits.max_iovecs =
			get_param(PTL_LIM_MAX_IOVECS);
		ni->limits.max_list_size =
			get_param(PTL_LIM_MAX_LIST_SIZE);
		ni->limits.max_triggered_ops =
			get_param(PTL_LIM_MAX_TRIGGERED_OPS);
		ni->limits.max_msg_size =
			get_param(PTL_LIM_MAX_MSG_SIZE);
		ni->limits.max_atomic_size =
			get_param(PTL_LIM_MAX_ATOMIC_SIZE);
		ni->limits.max_fetch_atomic_size =
			get_param(PTL_LIM_MAX_FETCH_ATOMIC_SIZE);
		ni->limits.max_waw_ordered_size =
			get_param(PTL_LIM_MAX_WAW_ORDERED_SIZE);
		ni->limits.max_war_ordered_size =
			get_param(PTL_LIM_MAX_WAR_ORDERED_SIZE);
		ni->limits.max_volatile_size =
			get_param(PTL_LIM_MAX_VOLATILE_SIZE);
		ni->limits.features = 0;
	}
}

/* Release the buffers still on the send_list and recv_list. */
static void release_buffers(ni_t *ni)
{
	buf_t *buf;

	/* TODO: cleanup of the XT/XI and their buffers that might still
	 * be in flight. It's only usefull when something bad happens, so
	 * it's not critical. */

	while(!list_empty(&ni->rdma.recv_list)) {
		struct list_head *entry = ni->rdma.recv_list.next;
		list_del(entry);
		buf = list_entry(entry, buf_t, list);
		buf_put(buf);
	}
}

/*
 * init_pools - initialize resource pools for NI
 */
static int init_pools(ni_t *ni)
{
	int err;

	ni->mr_pool.cleanup = mr_cleanup;

	err = pool_init(&ni->mr_pool, "mr", sizeof(mr_t),
					POOL_MR, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->md_pool.cleanup = md_cleanup;

	err = pool_init(&ni->md_pool, "md", sizeof(md_t),
					POOL_MD, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	if (ni->options & PTL_NI_MATCHING) {
		ni->me_pool.init = me_init;
		ni->me_pool.cleanup = me_cleanup;

		err = pool_init(&ni->me_pool, "me", sizeof(me_t),
						POOL_ME, (obj_t *)ni);
		if (err) {
			WARN();
			return err;
		}
	} else {
		ni->le_pool.cleanup = le_cleanup;
		ni->le_pool.init = le_init;

		err = pool_init(&ni->le_pool, "le", sizeof(le_t),
						POOL_LE, (obj_t *)ni);
		if (err) {
			WARN();
			return err;
		}
	}

	ni->eq_pool.setup = eq_new;
	ni->eq_pool.cleanup = eq_cleanup;

	err = pool_init(&ni->eq_pool, "eq", sizeof(eq_t),
					POOL_EQ, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->ct_pool.init = ct_init;
	ni->ct_pool.fini = ct_fini;
	ni->ct_pool.setup = ct_new;
	ni->ct_pool.cleanup = ct_cleanup;

	err = pool_init(&ni->ct_pool, "ct", sizeof(ct_t),
					POOL_CT, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->buf_pool.setup = buf_setup;
	ni->buf_pool.init = buf_init;
	ni->buf_pool.fini = buf_fini;
	ni->buf_pool.cleanup = buf_cleanup;
	ni->buf_pool.slab_size = 128*1024;

	err = pool_init(&ni->buf_pool, "buf", real_buf_t_size(),
					POOL_BUF, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->conn_pool.init = conn_init;
	ni->conn_pool.fini = conn_fini;
	err = pool_init(&ni->conn_pool, "conn", sizeof(conn_t),
					POOL_BUF, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}


	return PTL_OK;
}

/*
 * create_tables
 *	initialize private rank table in NI
 *	we are not yet connected to remote QPs
 */
static int create_tables(ni_t *ni)
{
	int i;
	conn_t *conn;
	const ptl_size_t map_size = ni->logical.map_size;
	ptl_process_t *mapping = ni->logical.mapping;

	ni->logical.rank_table = calloc(map_size, sizeof(entry_t));
	if (!ni->logical.rank_table) {
		WARN();
		return PTL_NO_SPACE;
	}

	for (i = 0; i < map_size; i++) {
		entry_t *entry = &ni->logical.rank_table[i];

		entry->rank = i;
		entry->nid = mapping[i].phys.nid;
		entry->pid = mapping[i].phys.pid;

		if (conn_alloc(ni, &entry->connect))
			return PTL_FAIL;

		/* convert nid/pid to ipv4 address */
		conn = entry->connect;
		conn->sin.sin_family = AF_INET;
		conn->sin.sin_addr.s_addr = nid_to_addr(entry->nid);
		conn->sin.sin_port = pid_to_port(entry->pid);
	}

	return PTL_OK;
}

int PtlNIInit(ptl_interface_t	iface_id,
	      unsigned int	options,
	      ptl_pid_t		pid,
	      const ptl_ni_limits_t *desired,
	      ptl_ni_limits_t	*actual,
	      ptl_handle_ni_t	*ni_handle)
{
	int err;
	ni_t *ni;
	int ni_type;
	iface_t *iface;
	gbl_t *gbl = &per_proc_gbl;

	err = gbl_get();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	iface = get_iface(gbl, iface_id);
	if (unlikely(!iface)) {
		err = PTL_NO_INIT;
		WARN();
		goto err1;
	}

	if (unlikely(options & ~PTL_NI_INIT_OPTIONS_MASK)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!!(options & PTL_NI_MATCHING)
				 ^ !(options & PTL_NI_NO_MATCHING))) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!!(options & PTL_NI_LOGICAL)
				 ^ !(options & PTL_NI_PHYSICAL))) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni_type = ni_options_to_type(options);

	pthread_mutex_lock(&gbl->gbl_mutex);

	/* check to see if ni of type ni_type already exists */
	ni = iface_get_ni(iface, ni_type);
	if (ni)
		goto done;

	/* check to see if iface is configured and waiting for cm events
	   if not, initialize it */
	err = init_iface_ib(iface);
	if (err) {
		WARN();
		goto err2;
	}

	err = ni_alloc(&gbl->ni_pool, &ni);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	OBJ_NEW(ni);

	/* Set the NID/PID for that NI. */
	ni->id.phys.nid = PTL_NID_ANY;
	ni->id.phys.pid = pid;

	if (pid == PTL_PID_ANY && iface->id.phys.pid != PTL_PID_ANY) {
		ni->id.phys.pid = iface->id.phys.pid;
	} else if (iface->id.phys.pid == PTL_PID_ANY && pid != PTL_PID_ANY) {
		iface->id.phys.pid = pid;

		ptl_info("set iface pid(2) = %x\n", iface->id.phys.pid);
	} else if (pid != iface->id.phys.pid) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	ni->iface = iface;
	ni->ni_type = ni_type;
	atomic_set(&ni->ref_cnt, 1);
	ni->options = options;
	ni->last_pt = -1;
	set_limits(ni, desired);
	ni->uid = geteuid();
	INIT_LIST_HEAD(&ni->md_list);
	INIT_LIST_HEAD(&ni->ct_list);
	RB_INIT(&ni->mr_tree);
	ni->umn_fd = -1;
	INIT_LIST_HEAD(&ni->rdma.recv_list);
	atomic_set(&ni->rdma.num_conn, 0);
	PTL_FASTLOCK_INIT(&ni->md_list_lock);
	PTL_FASTLOCK_INIT(&ni->ct_list_lock);
	PTL_FASTLOCK_INIT(&ni->mr_tree_lock);
	PTL_FASTLOCK_INIT(&ni->rdma.recv_list_lock);
	pthread_mutex_init(&ni->atomic_mutex, NULL);
	pthread_mutex_init(&ni->pt_mutex, NULL);
	if (options & PTL_NI_PHYSICAL) {
		PTL_FASTLOCK_INIT(&ni->physical.lock);
	}

	mr_init(ni);

	/* Note: pt range is [0..max_pt_index]. */
	ni->pt = calloc(ni->limits.max_pt_index + 1, sizeof(*ni->pt));
	if (unlikely(!ni->pt)) {
		WARN();
		err = PTL_NO_SPACE;
		goto err3;
	}

	err = init_pools(ni);
	if (unlikely(err))
		goto err3;

	err = PtlNIInit_IB(iface, ni);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	if (get_param(PTL_ENABLE_SHMEM)) {
		err = PtlNIInit_shmem(ni);
		if (unlikely(err)) {
			WARN();
			goto err3;
		}

		if (options & PTL_NI_PHYSICAL) {
			err = setup_shmem(ni);
			if (unlikely(err)) {
				WARN();
				goto err3;
			}
		}
	}

	/* Add a progress thread. */
	err = pthread_create(&ni->catcher, NULL, progress_thread, ni);
	if (err) {
		WARN();
		err = PTL_FAIL;
		goto err3;
	}
	ni->has_catcher = 1;

	iface_add_ni(iface, ni);

 done:
	pthread_mutex_unlock(&gbl->gbl_mutex);

	if (actual)
		*actual = ni->limits;

	*ni_handle = ni_to_handle(ni);

	gbl_put();
	return PTL_OK;

 err3:
	ni_put(ni);
 err2:
	pthread_mutex_unlock(&gbl->gbl_mutex);
 err1:
	gbl_put();
	return err;
}

int PtlSetMap(ptl_handle_ni_t ni_handle,
			  ptl_size_t	  map_size,
			  const ptl_process_t  *mapping)
{
	int err;
	ni_t *ni;
	iface_t *iface;
	int length;
	int i;

	err = gbl_get();
	if (unlikely(err)) {
		return err;
	}

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (ni->logical.mapping) {
		ni_put(ni);
		gbl_put();
		
		return PTL_IGNORED;
	}

	/* currently we must always create a physical NI first
	 * to establish the PID */
	if (ni->iface->id.phys.pid == PTL_PID_ANY) {
		ptl_warn("no PID established before creating logical NI\n");
		WARN();
		goto err2;
	}

	if ((ni->options & PTL_NI_LOGICAL) == 0) {
		/* Only valid on logical NIs. */
		goto err2;
	}

	/* Allocate new mapping and fill-up now. */
	length = map_size * sizeof(ptl_process_t);

	ni->logical.mapping = malloc(length);
	if (!ni->logical.mapping) {
		WARN();
		goto err2;
	}
	ni->logical.map_size = map_size;

	memcpy(ni->logical.mapping, mapping, length);

	/* lookup our nid/pid to determine rank */
	ni->id.rank = PTL_RANK_ANY;

	iface = ni->iface;
	for (i = 0; i < map_size; i++) {
		if (mapping[i].phys.nid == iface->id.phys.nid) {

			if (mapping[i].phys.pid == iface->id.phys.pid)
				ni->id.rank = i;
		}
	}

	if (ni->id.rank == PTL_RANK_ANY) {
		WARN();
		free(ni->logical.mapping);
		ni->logical.map_size = 0;
		goto err2;
	}

	PtlSetMap_mem(ni, map_size, mapping);

	err = create_tables(ni);
	if (err) {
		WARN();
		goto err2;
	}

	if (get_param(PTL_ENABLE_SHMEM) &&
		(ni->options & PTL_NI_LOGICAL) &&
		setup_shmem(ni)) {
		WARN();
		goto err2;
	}

	ni_put(ni);
	gbl_put();
	return PTL_OK;

 err2:	
	ni_put(ni);
 err1:
	gbl_put();

	return PTL_ARG_INVALID;
}

int PtlGetMap(ptl_handle_ni_t ni_handle,
			  ptl_size_t	  map_size,
			  ptl_process_t	 *mapping,
			  ptl_size_t	 *actual_map_size)
{
	int err;
	ni_t *ni;
  
	err = gbl_get();
	if (unlikely(err)) {
		return err;
	}

	err = to_ni(ni_handle, &ni);
	if (unlikely(err)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if ((ni->options & PTL_NI_LOGICAL) == 0) {
		/* Only valid on logical NIs. */
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (!ni->logical.mapping) {
		err = PTL_NO_SPACE;
		goto err2;
	}

	if (map_size < ni->logical.map_size)
		map_size = ni->logical.map_size;

	if (map_size)
		memcpy(mapping, ni->logical.mapping, map_size * sizeof(ptl_process_t));

	if (actual_map_size)
		*actual_map_size = map_size;

	ni_put(ni);
	gbl_put();
	return PTL_OK;

 err2:	
	ni_put(ni);
 err1:
	gbl_put();

	return err;
}

static void interrupt_cts(ni_t *ni)
{
	struct list_head *l;
	ct_t *ct;

	PTL_FASTLOCK_LOCK(&ni->ct_list_lock);
	list_for_each(l, &ni->ct_list) {
		ct = list_entry(l, ct_t, list);
		ct->interrupt = 1;
	}
	PTL_FASTLOCK_UNLOCK(&ni->ct_list_lock);
}

static void ni_cleanup(ni_t *ni)
{
	ni->shutting_down = 1;
	__sync_synchronize();

	initiate_disconnect_all(ni);
	
	/* Stop the progress thread. */
	if (ni->has_catcher) {
		ni->catcher_stop = 1;
		pthread_join(ni->catcher, NULL);
		ni->has_catcher = 0;
	}

	destroy_conns(ni);

	interrupt_cts(ni);
	cleanup_mr_tree(ni);

	EVL_WATCH(ev_io_stop(evl.loop, &ni->rdma.async_watcher));

	cleanup_shmem(ni);
	cleanup_ib(ni);

	ni->iface = NULL;

	release_buffers(ni);

	if (ni->options & PTL_NI_LOGICAL) {
		if (ni->logical.mapping) {
			free(ni->logical.mapping);
			ni->logical.mapping = NULL;
		}
		if (ni->logical.rank_table) {
			free(ni->logical.rank_table);
			ni->logical.rank_table = NULL;
		}
	}

	pool_fini(&ni->conn_pool);
	pool_fini(&ni->buf_pool);
	pool_fini(&ni->xt_pool);
	pool_fini(&ni->ct_pool);
	pool_fini(&ni->eq_pool);
	pool_fini(&ni->le_pool);
	pool_fini(&ni->me_pool);
	pool_fini(&ni->md_pool);
	pool_fini(&ni->mr_pool);

	if (ni->pt) {
		free(ni->pt);
		ni->pt = NULL;
	}

	pthread_mutex_destroy(&ni->atomic_mutex);
	pthread_mutex_destroy(&ni->pt_mutex);
	PTL_FASTLOCK_DESTROY(&ni->md_list_lock);
	PTL_FASTLOCK_DESTROY(&ni->ct_list_lock);
	PTL_FASTLOCK_DESTROY(&ni->mr_tree_lock);
	PTL_FASTLOCK_DESTROY(&ni->rdma.recv_list_lock);
}

int PtlNIFini(ptl_handle_ni_t ni_handle)
{
	int err;
	ni_t *ni;
	gbl_t *gbl = &per_proc_gbl;

	err = gbl_get();
	if (unlikely(err)) {
		return err;
	}

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	pthread_mutex_lock(&gbl->gbl_mutex);
	if (atomic_dec(&ni->ref_cnt) <= 0) {
		iface_remove_ni(ni);
		ni_cleanup(ni);
		ni_put(ni);
	}
	pthread_mutex_unlock(&gbl->gbl_mutex);

	ni_put(ni);
	gbl_put();
	return PTL_OK;

err1:
	gbl_put();
	return err;
}

int PtlNIStatus(ptl_handle_ni_t ni_handle, ptl_sr_index_t index,
		ptl_sr_value_t *status)
{
	int err;
	ni_t *ni;

	err = gbl_get();
	if (unlikely(err))
		return err;

	if (unlikely(index >= PTL_SR_LAST)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	*status = ni->status[index];

	ni_put(ni);
	gbl_put();
	return PTL_OK;

err1:
	gbl_put();
	return err;
}

int PtlNIHandle(ptl_handle_any_t handle, ptl_handle_ni_t *ni_handle)
{
	obj_t *obj;
	int err;

	err = gbl_get();
	if (unlikely(err))
		return err;

	obj = to_obj(POOL_ANY, handle);
	if (unlikely(!obj)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (!obj) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	*ni_handle = ni_to_handle(obj_to_ni(obj));

	obj_put(obj);
	gbl_put();
	return PTL_OK;

err1:
	gbl_put();
	return err;
}

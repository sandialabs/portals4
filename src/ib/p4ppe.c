/**
 * @file p4ppe.c
 *
 * Portals Process Engine main file.
 */

#include "ptl_loc.h"

/* Event loop. */
struct evl evl;

/* Hash table to keep tables of logical NIs, indexed by the lower
 * byte of crc32. */
struct logical_group {
	struct list_head list;
	ni_t **ni;
	uint32_t hash;
	int members;				/* number of rank in the group */
};

struct ppe {

	/* Communication pad. */
	struct {
		int size;
		int name;

		/* The communication pad in shared memory. */
		struct ppe_comm_pad *comm_pad;

	} comm_pad_info;

	/* When to stop the progress thread. */
	int stop;

	/* Internal queue. Used for communication regarding transfer
	 * between clients. */
	queue_t internal_queue;

	pthread_t		event_thread;
	int			event_thread_run;

	/* Pool/list of ppebufs, used by client to talk to PPE. */
	struct {
		void *slab;
		struct xpmem_map slab_mapping;
		int num;				/* total number of ppebufs */
	} ppebuf;

	/* Hash table to keep tables of logical tables, indexed by the
	 * lower byte of crc32. */
	struct list_head logical_group_list[0x100];
} ppe;

/* The communication pad for the PPE is only 4KB, and only contains a
 * queue structure. There is no buffers */
static int init_ppe(void)
{
	int shm_fd = -1;

	/* Our own queue. */
	queue_init(&ppe.internal_queue);

	ppe.comm_pad_info.size = 4096;	/* bigger than a queue_t */

	/* Just in case, remove that file if it already exist. */
	shm_unlink(COMM_PAD_FNAME);

	shm_fd = shm_open(COMM_PAD_FNAME,
					  O_RDWR | O_CREAT | O_EXCL,
					  S_IRUSR | S_IWUSR);
	assert(shm_fd >= 0);

	if (shm_fd < 0) {
		ptl_warn("shm_open of %s failed (errno=%d)",
				 COMM_PAD_FNAME, errno);
		goto exit_fail;
	}

	/* Enlarge the memory zone to the size we need. */
	if (ftruncate(shm_fd, ppe.comm_pad_info.size) != 0) {
		ptl_warn("share memory ftruncate failed");
		shm_unlink(COMM_PAD_FNAME);
		goto exit_fail;
	}

	/* Fill our portion of the comm pad. */
	ppe.comm_pad_info.comm_pad = mmap(NULL, ppe.comm_pad_info.size,
							   PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (ppe.comm_pad_info.comm_pad == MAP_FAILED) {
		ptl_warn("mmap failed (%d)", errno);
		perror("");
		goto exit_fail;
	}

	/* The share memory is mmaped, so we can close the file. */
	close(shm_fd);
	shm_fd = -1;

	/* Now we can create the buffer pool */
	queue_init(&ppe.comm_pad_info.comm_pad->queue);

 exit_fail:
	if (shm_fd != -1)
		close(shm_fd);

	return PTL_FAIL;
}

//todo: merge with send_message_shmem
static int send_message_mem(buf_t *buf, int from_init)
{
	/* Keep a reference on the buffer so it doesn't get freed. will be
	 * returned by the remote side with type=BUF_SHMEM_RETURN. */ 
	assert(buf->obj.obj_pool->type == POOL_BUF);
	buf_get(buf);

	buf->type = BUF_MEM_SEND;
	buf->obj.next = NULL;

	enqueue(NULL, &ppe.internal_queue, (obj_t *)buf);

	return PTL_OK;
}

static void mem_set_send_flags(buf_t *buf, int can_inline)
{
	/* The data is always in the buffer. */
	buf->event_mask |= XX_INLINE;
}

static void append_init_data_ppe_direct(data_t *data, mr_t *mr, void *addr,
										ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_MEM_DMA;

	data->mem.num_mem_iovecs = 1;
	data->mem.mem_iovec[0].addr = addr;
	data->mem.mem_iovec[0].length = length;

	buf->length += sizeof(*data) + sizeof(struct mem_iovec);
}

/* Let the remote side know where we store the IOVECS. This is used
 * for both direct and indirect iovecs cases. That avoids a copy into
 * the message buffer. */
static void append_init_data_ppe_iovec_direct_indirect(data_t *data, md_t *md, int iov_start,
												int num_iov, ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_MEM_INDIRECT;
	data->mem.num_mem_iovecs = num_sge;

	data->mem.mem_iovec[0].addr = &md->mem_iovecs[iov_start];
	data->mem.mem_iovec[0].length = num_iov * sizeof(struct mem_iovec);

	buf->length += sizeof(*data) + sizeof(struct mem_iovec);
}

static int ppe_tgt_data_out(buf_t *buf, data_t *data)
{
	switch(data->data_fmy) {
	case DATA_FMT_MEM_DMA:
		buf->transfer.mem.cur_rem_iovec = &data->mem.mem_iovec[0];
		buf->transfer.mem.num_rem_iovecs = data->mem.num_mem_iovecs;
		buf->transfer.mem.cur_rem_off = 0;

		next = STATE_TGT_RDMA;
	
		break;

	case DATA_FMT_MEM_INDIRECT:
		buf->transfer.mem.cur_rem_iovec = data->mem.mem_iovec[0].addr;
		buf->transfer.mem.num_rem_iovecs = data->mem.num_mem_iovecs;
		buf->transfer.mem.cur_rem_off = 0;

		next = STATE_TGT_RDMA;
		break;

	default:
		assert(0);
		WARN();
		next = STATE_TGT_ERROR;
	}
}

struct transport transport_mem = {
	.type = CONN_TYPE_MEM,
	.buf_alloc = buf_alloc,
	.post_tgt_dma = do_mem_transfer,
	.send_message = send_message_mem,
	.set_send_flags = mem_set_send_flags,
	.append_init_data_direct = append_init_data_ppe_direct,
	.append_init_data_iovec_direct = append_init_data_ppe_iovec_direct_indirect,
	.append_init_data_iovec_indirect = append_init_data_ppe_iovec_direct_indirect,
	.tgt_data_out = ppe_tgt_data_out,
};

/* Return object to client. The PPE cannot access that buf afterwards. */
static inline void buf_completed(ppebuf_t *buf)
{
	buf->completed = 1;
	__sync_synchronize();
}

static void do_OP_PtlInit(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlInit(&client->gbl);
}

static void do_OP_PtlFini(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	_PtlFini(&client->gbl);
}

static void do_OP_PtlNIInit(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlNIInit(&client->gbl,
							  buf->msg.PtlNIInit.iface,
							  buf->msg.PtlNIInit.options,
							  buf->msg.PtlNIInit.pid,
							  buf->msg.PtlNIInit.with_desired ?
							  &buf->msg.PtlNIInit.desired : NULL,
							  &buf->msg.PtlNIInit.actual,
							  &buf->msg.PtlNIInit.ni_handle);

	if (buf->msg.ret)
		WARN();
}

static void do_OP_PtlNIStatus(ppebuf_t *buf)
{
	buf->msg.ret = PtlNIStatus(buf->msg.PtlNIStatus.ni_handle,
							   buf->msg.PtlNIStatus.status_register,
							   &buf->msg.PtlNIStatus.status);
}

static struct logical_group *get_ni_group(unsigned int hash)
{
	unsigned int key = hash & 0xff;
	struct list_head *l;

	list_for_each(l, &ppe.logical_group_list[key]) {
		struct logical_group *group = list_entry(l, struct logical_group, list);

		if (group->hash == hash)
			return group;
	}

	return NULL;
}

/* Find the NI group for an incoming buffer. */
static ni_t *get_dest_ni(buf_t *mem_buf)
{
	struct ptl_hdr *hdr = mem_buf->data;
	struct logical_group *group;

	group = get_ni_group(le32_to_cpu(hdr->hash));

	if (!group)
		return NULL;

	return group->ni[le32_to_cpu(hdr->dst_rank)];
}

/* Remove an NI from a PPE group. */
static void remove_ni_from_group(ptl_handle_ni_t ni_handle)
{
	int err;
	ni_t *ni;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err)) {
		WARN();
		return;
	}

	if (!ni->mem.in_group)
		return;

	ni->mem.in_group = 0;

	if (ni->options & PTL_NI_PHYSICAL) {
		// todo
		abort();
	} else {
		struct logical_group *group;

		group = get_ni_group(ni->mem.hash);

		assert(group);
		assert(group->ni[ni->id.rank] == ni);

		group->ni[ni->id.rank] = NULL;
		group->members --;
		if (group->members == 0) {
			/* Remove group. */
			list_del(&group->list);

			free(group->ni);
			free(group);
		}
	}
}

static void do_OP_PtlNIFini(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	remove_ni_from_group(buf->msg.PtlNIFini.ni_handle);

	buf->msg.ret = _PtlNIFini(&client->gbl,
							  buf->msg.PtlNIFini.ni_handle);
	if (buf->msg.ret)
		WARN();
}

static void do_OP_PtlNIHandle(ppebuf_t *buf)
{
	buf->msg.ret = PtlNIHandle(buf->msg.PtlNIHandle.handle,
							   &buf->msg.PtlNIHandle.ni_handle);
}

static void insert_ni_into_group(ptl_handle_ni_t ni_handle)
{
	ni_t *ni;
	int err;

	/* Insert the NI into the hash table. */
	err = to_ni(ni_handle, &ni);
	if (unlikely(err)) {
		/* Just created. Cannot happen. */
		abort();
	}

	if (ni->options & PTL_NI_PHYSICAL) {
		// todo
		abort();
	} else {
		struct logical_group *group;

		group = get_ni_group(ni->mem.hash);

		if (group == NULL) {
			//todo: should group be a pool?
			group = calloc(1, sizeof(*group));

			group->hash = ni->mem.hash;
			group->ni = calloc(sizeof (ni_t *), ni->logical.map_size);

			list_add_tail(&group->list, &ppe.logical_group_list[ni->mem.hash & 0xff]);
		} else {
			/* The group already exists. */
			if (group->ni[ni->id.rank]) {
				/* Error. There is something there already. Cannot
				 * happen, unless there is a hash collision. Hard to
				 * recover from. */
				abort();
			}
		}
				
		group->ni[ni->id.rank] = ni;
		group->members ++;

		ni->mem.in_group = 1;
	}		
	
	ni_put(ni);
}

static void do_OP_PtlSetMap(ppebuf_t *buf)
{
	ptl_process_t *mapping;

	mapping = map_segment(&buf->msg.PtlSetMap.mapping);
	if (mapping) {
		int ret;

		ret = PtlSetMap(buf->msg.PtlSetMap.ni_handle,
						buf->msg.PtlSetMap.map_size,
						mapping);

		buf->msg.ret = ret;

		if (ret == PTL_OK) {
			insert_ni_into_group(buf->msg.PtlSetMap.ni_handle);
		}

		unmap_segment(&buf->msg.PtlSetMap.mapping);
	} else {
		WARN();
		buf->msg.ret = PTL_ARG_INVALID;
	}
}

static void do_OP_PtlGetMap(ppebuf_t *buf)
{
	ptl_process_t *mapping;

	mapping = map_segment(&buf->msg.PtlGetMap.mapping);
	if (mapping) {
		buf->msg.ret = PtlGetMap(buf->msg.PtlSetMap.ni_handle,
								 buf->msg.PtlSetMap.map_size,
								 mapping,
								 &buf->msg.PtlGetMap.actual_map_size);

		unmap_segment(&buf->msg.PtlGetMap.mapping);
	}
}

static void do_OP_PtlPTAlloc(ppebuf_t *buf)
{
	buf->msg.ret = PtlPTAlloc(buf->msg.PtlPTAlloc.ni_handle,
							  buf->msg.PtlPTAlloc.options,
							  buf->msg.PtlPTAlloc.eq_handle,
							  buf->msg.PtlPTAlloc.pt_index_req,
							  &buf->msg.PtlPTAlloc.pt_index);
}

static void do_OP_PtlPTFree(ppebuf_t *buf)
{
	buf->msg.ret = PtlPTFree(buf->msg.PtlPTFree.ni_handle,
							 buf->msg.PtlPTFree.pt_index);
}

static void do_OP_PtlMESearch(ppebuf_t *buf)
{
	buf->msg.ret = PtlMESearch(buf->msg.PtlMESearch.ni_handle,
							   buf->msg.PtlMESearch.pt_index,
							   &buf->msg.PtlMESearch.me,
							   buf->msg.PtlMESearch.ptl_search_op,
							   buf->msg.PtlMESearch.user_ptr);
}

/* Destroy mapping created by create_local_iovecs(). */
static void destroy_local_iovecs(struct xpmem_map *mappings, int num_iov)
{
	int i;

	for (i=0; i<num_iov; i++) {
		unmap_segment(&mappings[i]);
	}
}

/* Given a mapping, recreate a set of iovecs in our own address
 * space. */
static ptl_iovec_t *create_local_iovecs(struct xpmem_map *mappings, int num_iov)
{
	int i;
	ptl_iovec_t *iovecs;

	iovecs = calloc(num_iov, sizeof(ptl_iovec_t));
	if (!iovecs) {
		return NULL;
	}

	for (i=0; i<num_iov; i++) {
		ptl_iovec_t *iovec = &iovecs[i];

		iovec->iov_base = map_segment(&mappings[i]);
		if (!iovec->iov_base)
			goto err;

		iovec->iov_len = mappings[i].size;
	}

	return iovecs;

 err:
	destroy_local_iovecs(mappings, num_iov);
	free(iovecs);

	return NULL;
}

static void do_OP_PtlMEAppend(ppebuf_t *buf)
{
	void *start;
	struct ptl_me_ppe me_init_ppe;
	ptl_iovec_t *iovecs = NULL;

	start = map_segment(&buf->msg.PtlMEAppend.mapping);

	if (start) {

		me_init_ppe.me_init = buf->msg.PtlMEAppend.me;
		me_init_ppe.mapping = buf->msg.PtlMEAppend.mapping;

		if (buf->msg.PtlMEAppend.me.options & PTL_IOVEC) {
			iovecs = create_local_iovecs(start, buf->msg.PtlMEAppend.me.length);
			if (!iovecs) {
				unmap_segment(&buf->msg.PtlMEAppend.mapping);

				buf->msg.ret = PTL_NO_SPACE;
				return;
			}

			me_init_ppe.iovecs_mappings = start;
			
			/* Fix the start pointer, by touching the buffer source. */
			me_init_ppe.me_init.start = iovecs;

		} else {
			/* Fix the start pointer, by touching the buffer source. */
			me_init_ppe.me_init.start = start;
		}

		buf->msg.ret = PtlMEAppend(buf->msg.PtlMEAppend.ni_handle,
								   buf->msg.PtlMEAppend.pt_index,
								   &me_init_ppe.me_init,
								   buf->msg.PtlMEAppend.ptl_list,
								   buf->msg.PtlMEAppend.user_ptr,
								   &buf->msg.PtlMEAppend.me_handle);

		if (buf->msg.ret != PTL_OK) {
			unmap_segment(&buf->msg.PtlMEAppend.mapping);

			// todo
			//	if (iovecs)
			//					destroy_local_iovecs(buf->msg.PtlMEAppend.mapping);
		}
	} else {
		WARN();
		buf->msg.ret = PTL_ARG_INVALID;
	}
}

static void do_OP_PtlMEUnlink(ppebuf_t *buf)
{
	me_t *me;
	struct xpmem_map mapping;

	if (to_me(buf->msg.PtlMEUnlink.me_handle, &me) != PTL_OK) {
		/* Already freed? Whatever. */
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}

	mapping = me->ppe.mapping;
	me_put(me);					/* from to_md() */

	buf->msg.ret = PtlMEUnlink(buf->msg.PtlMEUnlink.me_handle);

	if (buf->msg.ret == PTL_OK) {
		unmap_segment(&mapping);
	}	
}

static void do_OP_PtlLEAppend(ppebuf_t *buf)
{
	void *start;
	struct ptl_le_ppe le_init_ppe;
	ptl_iovec_t *iovecs = NULL;

	start = map_segment(&buf->msg.PtlLEAppend.mapping);

	if (start) {
		le_init_ppe.le_init = buf->msg.PtlLEAppend.le;
		le_init_ppe.mapping = buf->msg.PtlLEAppend.mapping;

		if (buf->msg.PtlLEAppend.le.options & PTL_IOVEC) {
			iovecs = create_local_iovecs(start, buf->msg.PtlLEAppend.le.length);
			if (!iovecs) {
				unmap_segment(&buf->msg.PtlLEAppend.mapping);

				buf->msg.ret = PTL_NO_SPACE;
				return;
			}
			
			le_init_ppe.iovecs_mappings = start;

			/* Fix the start pointer, by touching the buffer source. */
			le_init_ppe.le_init.start = iovecs;

		} else {
			/* Fix the start pointer, by touching the buffer source. */
			le_init_ppe.le_init.start = start;
		}

		buf->msg.ret = PtlLEAppend(buf->msg.PtlLEAppend.ni_handle,
								   buf->msg.PtlLEAppend.pt_index,
								   &le_init_ppe.le_init,
								   buf->msg.PtlLEAppend.ptl_list,
								   buf->msg.PtlLEAppend.user_ptr,
								   &buf->msg.PtlLEAppend.le_handle);

		if (buf->msg.ret != PTL_OK) {
			unmap_segment(&buf->msg.PtlLEAppend.mapping);

			// todo
			//	if (iovecs)
			//					destroy_local_iovecs(buf->msg.PtlLEAppend.mapping);
		}
	} else {
		WARN();
		buf->msg.ret = PTL_ARG_INVALID;
	}
}

static void do_OP_PtlLESearch(ppebuf_t *buf)
{
	buf->msg.ret = PtlLESearch(	buf->msg.PtlLESearch.ni_handle,
								buf->msg.PtlLESearch.pt_index,
								&buf->msg.PtlLESearch.le,
								buf->msg.PtlLESearch.ptl_search_op,
								buf->msg.PtlLESearch.user_ptr);
}

static void do_OP_PtlLEUnlink(ppebuf_t *buf)
{
	le_t *le;
	struct xpmem_map mapping;

	if (to_le(buf->msg.PtlLEUnlink.le_handle, &le) != PTL_OK) {
		/* Already freed? Whatever. */
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}

	mapping = le->ppe.mapping;
	le_put(le);					/* from to_md() */

	buf->msg.ret = PtlLEUnlink(buf->msg.PtlLEUnlink.le_handle);

	if (buf->msg.ret == PTL_OK) {
		unmap_segment(&mapping);

		// todo
		//	if (iovecs)
		//					destroy_local_iovecs(buf->msg.PtlLEAppend.mapping);
	}	
}

static void do_OP_PtlMDBind(ppebuf_t *buf)
{
	void *start;
	struct ptl_md_ppe md_init_ppe;
	ptl_iovec_t *iovecs = NULL;

	start = map_segment(&buf->msg.PtlMDBind.mapping);

	if (start) {
		md_init_ppe.md_init = buf->msg.PtlMDBind.md;
		md_init_ppe.mapping = buf->msg.PtlMDBind.mapping;

		if (buf->msg.PtlMDBind.md.options & PTL_IOVEC) {
			iovecs = create_local_iovecs(start, buf->msg.PtlMDBind.md.length);
			if (!iovecs) {
				unmap_segment(&buf->msg.PtlMDBind.mapping);

				buf->msg.ret = PTL_NO_SPACE;
				return;
			}

			md_init_ppe.iovecs_mappings = start;

			/* Fix the start pointer, by touching the buffer source. */
			md_init_ppe.md_init.start = iovecs;
		} else {
			/* Fix the start pointer, by touching the buffer source. */
			md_init_ppe.md_init.start = start;
		}

		buf->msg.ret = PtlMDBind(buf->msg.PtlMDBind.ni_handle,
								 &md_init_ppe.md_init,
								 &buf->msg.PtlMDBind.md_handle);
	} else {
		WARN();
		buf->msg.ret = PTL_ARG_INVALID;
	}
}

static void do_OP_PtlMDRelease(ppebuf_t *buf)
{
	md_t *md = to_md(buf->msg.PtlMDRelease.md_handle);
	struct xpmem_map mapping;

	if (!md) {
		/* Already freed? Whatever. */
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}

	mapping = md->ppe.mapping;
	md_put(md);					/* from to_md() */

	buf->msg.ret = PtlMDRelease(buf->msg.PtlMDRelease.md_handle);

	if (buf->msg.ret == PTL_OK) {
		unmap_segment(&mapping);
	}
}

static void do_OP_PtlGetId(ppebuf_t *buf)
{
	buf->msg.ret = PtlGetId(buf->msg.PtlGetId.ni_handle,
							&buf->msg.PtlGetId.id);
}

static void do_OP_PtlPut(ppebuf_t *buf)
{
	buf->msg.ret = PtlPut(buf->msg.PtlPut.md_handle,
						  buf->msg.PtlPut.local_offset,
						  buf->msg.PtlPut.length,
						  buf->msg.PtlPut.ack_req,
						  buf->msg.PtlPut.target_id,
						  buf->msg.PtlPut.pt_index,
						  buf->msg.PtlPut.match_bits,
						  buf->msg.PtlPut.remote_offset,
						  buf->msg.PtlPut.user_ptr,
						  buf->msg.PtlPut.hdr_data);
}

static void do_OP_PtlGet(ppebuf_t *buf)
{
	buf->msg.ret = PtlGet(buf->msg.PtlGet.md_handle,
						  buf->msg.PtlGet.local_offset,
						  buf->msg.PtlGet.length,
						  buf->msg.PtlGet.target_id,
						  buf->msg.PtlGet.pt_index,
						  buf->msg.PtlGet.match_bits,
						  buf->msg.PtlGet.remote_offset,
						  buf->msg.PtlGet.user_ptr);
}

static void do_OP_PtlAtomic(ppebuf_t *buf)
{
	buf->msg.ret = PtlAtomic(buf->msg.PtlAtomic.md_handle,
							 buf->msg.PtlAtomic.local_offset,
							 buf->msg.PtlAtomic.length,
							 buf->msg.PtlAtomic.ack_req,
							 buf->msg.PtlAtomic.target_id,
							 buf->msg.PtlAtomic.pt_index,
							 buf->msg.PtlAtomic.match_bits,
							 buf->msg.PtlAtomic.remote_offset,
							 buf->msg.PtlAtomic.user_ptr,
							 buf->msg.PtlAtomic.hdr_data,
							 buf->msg.PtlAtomic.operation,
							 buf->msg.PtlAtomic.datatype);
}

static void do_OP_PtlFetchAtomic(ppebuf_t *buf)
{
	buf->msg.ret = PtlFetchAtomic(buf->msg.PtlFetchAtomic.get_md_handle,
								  buf->msg.PtlFetchAtomic.local_get_offset,
								  buf->msg.PtlFetchAtomic.put_md_handle,
								  buf->msg.PtlFetchAtomic.local_put_offset,
								  buf->msg.PtlFetchAtomic.length,
								  buf->msg.PtlFetchAtomic.target_id,
								  buf->msg.PtlFetchAtomic.pt_index,
								  buf->msg.PtlFetchAtomic.match_bits,
								  buf->msg.PtlFetchAtomic.remote_offset,
								  buf->msg.PtlFetchAtomic.user_ptr,
								  buf->msg.PtlFetchAtomic.hdr_data,
								  buf->msg.PtlFetchAtomic.operation,
								  buf->msg.PtlFetchAtomic.datatype);
}

static void do_OP_PtlSwap(ppebuf_t *buf)
{
	buf->msg.ret = PtlSwap(buf->msg.PtlSwap.get_md_handle,
						   buf->msg.PtlSwap.local_get_offset,
						   buf->msg.PtlSwap.put_md_handle,
						   buf->msg.PtlSwap.local_put_offset,
						   buf->msg.PtlSwap.length,
						   buf->msg.PtlSwap.target_id,
						   buf->msg.PtlSwap.pt_index,
						   buf->msg.PtlSwap.match_bits,
						   buf->msg.PtlSwap.remote_offset,
						   buf->msg.PtlSwap.user_ptr,
						   buf->msg.PtlSwap.hdr_data,
						   buf->msg.PtlSwap.operand,
						   buf->msg.PtlSwap.operation,
						   buf->msg.PtlSwap.datatype);
}

static void do_OP_PtlTriggeredPut(ppebuf_t *buf)
{
	buf->msg.ret = PtlTriggeredPut(buf->msg.PtlTriggeredPut.md_handle,
								   buf->msg.PtlTriggeredPut.local_offset,
								   buf->msg.PtlTriggeredPut.length,
								   buf->msg.PtlTriggeredPut.ack_req,
								   buf->msg.PtlTriggeredPut.target_id,
								   buf->msg.PtlTriggeredPut.pt_index,
								   buf->msg.PtlTriggeredPut.match_bits,
								   buf->msg.PtlTriggeredPut.remote_offset,
								   buf->msg.PtlTriggeredPut.user_ptr,
								   buf->msg.PtlTriggeredPut.hdr_data,
								   buf->msg.PtlTriggeredPut.trig_ct_handle,
								   buf->msg.PtlTriggeredPut.threshold);
}


static void do_OP_PtlTriggeredGet(ppebuf_t *buf)
{
	buf->msg.ret = PtlTriggeredGet(buf->msg.PtlTriggeredGet.md_handle,
								   buf->msg.PtlTriggeredGet.local_offset,
								   buf->msg.PtlTriggeredGet.length,
								   buf->msg.PtlTriggeredGet.target_id,
								   buf->msg.PtlTriggeredGet.pt_index,
								   buf->msg.PtlTriggeredGet.match_bits,
								   buf->msg.PtlTriggeredGet.remote_offset,
								   buf->msg.PtlTriggeredGet.user_ptr,
								   buf->msg.PtlTriggeredGet.trig_ct_handle,
								   buf->msg.PtlTriggeredGet.threshold);
}

static void do_OP_PtlTriggeredAtomic(ppebuf_t *buf)
{
	buf->msg.ret = PtlTriggeredAtomic(buf->msg.PtlTriggeredAtomic.md_handle,
									  buf->msg.PtlTriggeredAtomic.local_offset,
									  buf->msg.PtlTriggeredAtomic.length,
									  buf->msg.PtlTriggeredAtomic.ack_req,
									  buf->msg.PtlTriggeredAtomic.target_id,
									  buf->msg.PtlTriggeredAtomic.pt_index,
									  buf->msg.PtlTriggeredAtomic.match_bits,
									  buf->msg.PtlTriggeredAtomic.remote_offset,
									  buf->msg.PtlTriggeredAtomic.user_ptr,
									  buf->msg.PtlTriggeredAtomic.hdr_data,
									  buf->msg.PtlTriggeredAtomic.operation,
									  buf->msg.PtlTriggeredAtomic.datatype,
									  buf->msg.PtlTriggeredAtomic.trig_ct_handle,
									  buf->msg.PtlTriggeredAtomic.threshold);
}

static void do_OP_PtlTriggeredFetchAtomic(ppebuf_t *buf)
{
	buf->msg.ret = PtlTriggeredFetchAtomic(buf->msg.PtlTriggeredFetchAtomic.get_md_handle,
										   buf->msg.PtlTriggeredFetchAtomic.local_get_offset,
										   buf->msg.PtlTriggeredFetchAtomic.put_md_handle,
										   buf->msg.PtlTriggeredFetchAtomic.local_put_offset,
										   buf->msg.PtlTriggeredFetchAtomic.length,
										   buf->msg.PtlTriggeredFetchAtomic.target_id,
										   buf->msg.PtlTriggeredFetchAtomic.pt_index,
										   buf->msg.PtlTriggeredFetchAtomic.match_bits,
										   buf->msg.PtlTriggeredFetchAtomic.remote_offset,
										   buf->msg.PtlTriggeredFetchAtomic.user_ptr,
										   buf->msg.PtlTriggeredFetchAtomic.hdr_data,
										   buf->msg.PtlTriggeredFetchAtomic.operation,
										   buf->msg.PtlTriggeredFetchAtomic.datatype,
										   buf->msg.PtlTriggeredFetchAtomic.trig_ct_handle,
										   buf->msg.PtlTriggeredFetchAtomic.threshold);
}

static void do_OP_PtlTriggeredSwap(ppebuf_t *buf)
{
	buf->msg.ret = PtlTriggeredSwap(buf->msg.PtlTriggeredSwap.get_md_handle,
									buf->msg.PtlTriggeredSwap.local_get_offset,
									buf->msg.PtlTriggeredSwap.put_md_handle,
									buf->msg.PtlTriggeredSwap.local_put_offset,
									buf->msg.PtlTriggeredSwap.length,
									buf->msg.PtlTriggeredSwap.target_id,
									buf->msg.PtlTriggeredSwap.pt_index,
									buf->msg.PtlTriggeredSwap.match_bits,
									buf->msg.PtlTriggeredSwap.remote_offset,
									buf->msg.PtlTriggeredSwap.user_ptr,
									buf->msg.PtlTriggeredSwap.hdr_data,
									buf->msg.PtlTriggeredSwap.operand,
									buf->msg.PtlTriggeredSwap.operation,
									buf->msg.PtlTriggeredSwap.datatype,
									buf->msg.PtlTriggeredSwap.trig_ct_handle,
									buf->msg.PtlTriggeredSwap.threshold);
}

static void do_OP_PtlTriggeredCTInc(ppebuf_t *buf)
{
	buf->msg.ret = PtlTriggeredCTInc(buf->msg.PtlTriggeredCTInc.ct_handle,
									 buf->msg.PtlTriggeredCTInc.increment,
									 buf->msg.PtlTriggeredCTInc.trig_ct_handle,
									 buf->msg.PtlTriggeredCTInc.threshold);
}

static void do_OP_PtlTriggeredCTSet(ppebuf_t *buf)
{
	buf->msg.ret = PtlTriggeredCTSet(buf->msg.PtlTriggeredCTSet.ct_handle,
									 buf->msg.PtlTriggeredCTSet.new_ct,
									 buf->msg.PtlTriggeredCTSet.trig_ct_handle,
									 buf->msg.PtlTriggeredCTSet.threshold);
}

static void do_OP_PtlGetPhysId(ppebuf_t *buf)
{
	buf->msg.ret = PtlGetPhysId(buf->msg.PtlGetPhysId.ni_handle,
								&buf->msg.PtlGetPhysId.id);
}

static void do_OP_PtlEQAlloc(ppebuf_t *buf)
{
	buf->msg.ret = PtlEQAlloc(buf->msg.PtlEQAlloc.ni_handle,
							  buf->msg.PtlEQAlloc.count,
							  &buf->msg.PtlEQAlloc.eq_handle);

	if (buf->msg.ret == PTL_OK) {
		int err;
		eq_t *eq;

		/* Should not fail since it was just created. */
		err = to_eq(buf->msg.PtlEQAlloc.eq_handle, &eq);
		assert(err == PTL_OK);

		err = create_mapping(eq->eqe_list, eq->eqe_list_size,
							 &eq->ppe.eqe_list);
		buf->msg.PtlEQAlloc.eqe_list = eq->ppe.eqe_list;

		eq_put(eq);

		if (err != PTL_OK) {
			PtlEQFree(buf->msg.PtlEQAlloc.eq_handle);
			buf->msg.ret = PTL_ARG_INVALID;
			return;
		}
	}
}

static void do_OP_PtlEQFree(ppebuf_t *buf)
{
	int err;
	eq_t *eq;
	struct xpmem_map mapping;

	err = to_eq(buf->msg.PtlEQFree.eq_handle, &eq);
	if (err || !eq) {
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}
	mapping = eq->ppe.eqe_list;
	eq_put(eq);

	buf->msg.ret = PtlEQFree(buf->msg.PtlEQFree.eq_handle);

	if (buf->msg.ret == PTL_OK)
		delete_mapping(&mapping);
}

static void do_OP_PtlCTAlloc(ppebuf_t *buf)
{
	buf->msg.ret = PtlCTAlloc(buf->msg.PtlCTAlloc.ni_handle,
							  &buf->msg.PtlCTAlloc.ct_handle);

	if (buf->msg.ret == PTL_OK) {
		int err;
		ct_t *ct;

		/* Should not fail since it was just created. */
		err = to_ct(buf->msg.PtlCTAlloc.ct_handle, &ct);
		assert(err == PTL_OK);

		err = create_mapping(&ct->info, sizeof(struct ct_info),
							 &ct->ppe.ct_mapping);
		buf->msg.PtlCTAlloc.ct_mapping = ct->ppe.ct_mapping;

		ct_put(ct);

		if (err != PTL_OK) {
			PtlCTFree(buf->msg.PtlCTAlloc.ct_handle);
			buf->msg.ret = PTL_ARG_INVALID;
			return;
		}
	}
}

static void do_OP_PtlCTInc(ppebuf_t *buf)
{
	buf->msg.ret = PtlCTInc(buf->msg.PtlCTInc.ct_handle, buf->msg.PtlCTInc.increment);
}

static void do_OP_PtlCTSet(ppebuf_t *buf)
{
	buf->msg.ret = PtlCTSet(buf->msg.PtlCTSet.ct_handle, buf->msg.PtlCTSet.new_ct);
}

static void do_OP_PtlCTFree(ppebuf_t *buf)
{
	int err;
	ct_t *ct;
	struct xpmem_map mapping;

	err = to_ct(buf->msg.PtlCTFree.ct_handle, &ct);
	if (err || !ct) {
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}
	mapping = ct->ppe.ct_mapping;
	ct_put(ct);

	buf->msg.ret = PtlCTFree(buf->msg.PtlCTFree.ct_handle);

	if (buf->msg.ret == PTL_OK)
		delete_mapping(&mapping);
}

static void do_OP_PtlPTDisable(ppebuf_t *buf)
{
	buf->msg.ret = PtlPTDisable(buf->msg.PtlPTDisable.ni_handle,
								buf->msg.PtlPTDisable.pt_index);
}

static void do_OP_PtlPTEnable(ppebuf_t *buf)
{
	buf->msg.ret = PtlPTEnable(buf->msg.PtlPTDisable.ni_handle,
							   buf->msg.PtlPTDisable.pt_index);
}

static void do_OP_PtlAtomicSync(ppebuf_t *buf)
{
	buf->msg.ret = PtlAtomicSync();
}

static void do_OP_PtlCTCancelTriggered(ppebuf_t *buf)
{
	buf->msg.ret = PtlCTCancelTriggered(buf->msg.PtlCTCancelTriggered.ct_handle);
}

static void do_OP_PtlGetUid(ppebuf_t *buf)
{
	buf->msg.ret = PtlGetUid(buf->msg.PtlGetUid.ni_handle,
							 &buf->msg.PtlGetUid.uid);
}



#define ADD_OP(opname) [OP_##opname] = { .func = do_OP_##opname, .name = #opname }
static struct {
	void (*func)(ppebuf_t *buf);
	const char *name;
} ppe_ops[] = {
	ADD_OP(PtlAtomic),
	ADD_OP(PtlAtomicSync),
	ADD_OP(PtlCTAlloc),
	ADD_OP(PtlCTCancelTriggered),
	ADD_OP(PtlCTFree),
	ADD_OP(PtlCTInc),
	ADD_OP(PtlCTSet),
	ADD_OP(PtlEQAlloc),
	ADD_OP(PtlEQFree),
	ADD_OP(PtlFetchAtomic),
	ADD_OP(PtlFini),
	ADD_OP(PtlGet),
	ADD_OP(PtlGetId),
	ADD_OP(PtlGetMap),
	ADD_OP(PtlGetPhysId),
	ADD_OP(PtlGetUid),
	ADD_OP(PtlInit),
	ADD_OP(PtlLEAppend),
	ADD_OP(PtlLESearch),
	ADD_OP(PtlLEUnlink),
	ADD_OP(PtlMDBind),
	ADD_OP(PtlMDRelease),
	ADD_OP(PtlMEAppend),
	ADD_OP(PtlMESearch),
	ADD_OP(PtlMEUnlink),
	ADD_OP(PtlNIFini),
	ADD_OP(PtlNIHandle),
	ADD_OP(PtlNIInit),
	ADD_OP(PtlNIStatus),
	ADD_OP(PtlPTAlloc),
	ADD_OP(PtlPTDisable),
	ADD_OP(PtlPTEnable),
	ADD_OP(PtlPTFree),
	ADD_OP(PtlPut),
	ADD_OP(PtlSetMap),
	ADD_OP(PtlSwap),
	ADD_OP(PtlTriggeredAtomic),
	ADD_OP(PtlTriggeredCTInc),
	ADD_OP(PtlTriggeredCTSet),
	ADD_OP(PtlTriggeredFetchAtomic),
	ADD_OP(PtlTriggeredGet),
	ADD_OP(PtlTriggeredPut),
	ADD_OP(PtlTriggeredSwap),
};

/* List of existing clients. May replace with a tree someday. */
PTL_LIST_HEAD(clients);

static struct client *find_client(pid_t pid)
{
	struct list_head *l;

	list_for_each(l, &clients) {
		struct client *client = list_entry(l, struct client, list);

		if (client->pid == pid)
			return client;
	}

	return NULL;
}

/* Progress thread for the PPE. */
// TODO: tooo many spinlock_body here
static void ppe_progress(void)
{
	struct ppe_comm_pad * const comm_pad = ppe.comm_pad_info.comm_pad;

	while (!ppe.stop) {
		ppebuf_t *ppebuf;
		buf_t *mem_buf;
		struct client *client;

		/* Get init message from the comm pad. */
		switch(comm_pad->cmd.level) {
		case 0:
			/* Nothing to do. */
			break;

		case 1:
			/* A client is writing a command. */
			break;

		case 2:
			client = find_client(comm_pad->cmd.pid);
			if (client) {
				/* Already exists. So it means the old one went
				 * away. Destroy. */
				//todo
				abort();
			}

			client = calloc(1, sizeof(struct client));
			if (client) {
				client->pid = comm_pad->cmd.pid;
				list_add(&client->list, &clients);
			
				comm_pad->cmd.cookie = client;
				comm_pad->cmd.ppebufs_mapping = ppe.ppebuf.slab_mapping;
				comm_pad->cmd.ppebufs_ppeaddr = ppe.ppebuf.slab;
				comm_pad->cmd.ret = PTL_OK;
			} else {
				WARN();
				comm_pad->cmd.ret = PTL_FAIL;
			}

			/* Let the client read the result. */
			switch_cmd_level(comm_pad, 2, 3);

			break;

		case 3:
			/* Client's move. Nothing to do. */
			break;

		default:
			abort();
		}

		/* Get message from the message queue. */
		ppebuf = (ppebuf_t *)dequeue(NULL, &comm_pad->queue);

		if (ppebuf) {
			ppe_ops[ppebuf->op].func(ppebuf);

			/* Return response to blocked client. */
			buf_completed(ppebuf);
		}

		/* Get message from our own queue. */
		mem_buf = (buf_t *)dequeue(NULL, &ppe.internal_queue);
		if (mem_buf) {
			int err;

			if (mem_buf->type == BUF_MEM_SEND) {
				buf_t *buf;
				ni_t *ni;

				/* Mark it for releasing. The target state machine might
				 * change its type back to BUF_MEM_SEND. */
				mem_buf->type = BUF_MEM_RELEASE;

				/* Find the destination NI. */
				ni = get_dest_ni(mem_buf);				
				if (!ni) {
					/* Old packet ? Lost packet ? Drop it. todo. */
					abort();
				}

				err = buf_alloc(ni, &buf);
				if (err) {
					WARN();
				} else {
					buf->data = (hdr_t *)mem_buf->internal_data;
					buf->length = mem_buf->length;
					buf->mem_buf = mem_buf;
					INIT_LIST_HEAD(&buf->list);
					process_recv_mem(ni, buf);

					if (mem_buf->type == BUF_MEM_SEND) {
						err = mem_buf->conn->transport.send_message(mem_buf, 0);
						if (err) {
							WARN();
						}
					}
				}
			}
			else {
				assert(mem_buf->type == BUF_MEM_RELEASE);
			}

			/* From send_message_mem(). */
			buf_put(mem_buf);
		}

		/* TODO: don't spin if we got messages. */
		SPINLOCK_BODY();
	}
}

void gbl_release(ref_t *ref)
{
	gbl_t *gbl = container_of(ref, gbl_t, ref);

	/* cleanup ni object pool */
	pool_fini(&gbl->ni_pool);

	iface_fini(gbl);

	pthread_mutex_destroy(&gbl->gbl_mutex);
}

int gbl_init(gbl_t *gbl)
{
	int err;

	err = init_iface_table(gbl);
	if (err)
		return err;

	pthread_mutex_init(&gbl->gbl_mutex, NULL);

	/* init ni object pool */
	err = pool_init(&gbl->ni_pool, "ni", sizeof(ni_t), POOL_NI, NULL);
	if (err) {
		WARN();
		goto err;
	}

	return PTL_OK;

err:
	pthread_mutex_destroy(&gbl->gbl_mutex);
	return err;
}

static unsigned int fakepid(void)
{
	static int pid = 123;
	/*todo: check the pid is not already used, by going through the
	 * list of clients. */
	return pid++;
}

/**
 * @brief Initialize shared memory resources.
 *
 * @param[in] ni
 *
 * @return status
 */
int PtlNIInit_ppe(ni_t *ni)
{
	/* Only if IB hasn't setup the NID first. */
	if (ni->iface->id.phys.nid == PTL_NID_ANY) {
		//todo : bad
		ni->iface->id.phys.nid = 0;
	}
	if (ni->iface->id.phys.pid == PTL_PID_ANY)
		ni->iface->id.phys.pid = fakepid();

	ni->id.phys.nid = ni->iface->id.phys.nid;

	if (ni->id.phys.pid == PTL_PID_ANY)
		ni->id.phys.pid = ni->iface->id.phys.pid;

	return PTL_OK;
}

#if 0
static void stop_event_loop_func(EV_P_ ev_async *w, int revents)
{
	ev_break(evl.loop, EVBREAK_ALL);
}
#endif

static void *event_loop_func(void *arg)
{
	evl_run(&evl);
	return NULL;
}

/* Create a unique ppebuf pool, to be used/shared by all clients. */
static int setup_ppebufs(void)
{
	size_t size;
	int ret;
	pool_t *pool = &ppe.comm_pad_info.comm_pad->ppebuf_pool;

	ppe.ppebuf.num = 1000;

	pool->use_pre_alloc_buffer = 1;
	pool->slab_size = ppe.ppebuf.num * sizeof(ppebuf_t);

	/* Round up to page size. */
	size = (pool->slab_size + pagesize - 1) & ~(pagesize-1);

	if (posix_memalign((void **)&ppe.ppebuf.slab, pagesize, size)) {
		WARN();
		ppe.ppebuf.slab = NULL;
		return 1;
	}

	/* Make the communication pad shareable through XPMEM. */
	ret = create_mapping(ppe.ppebuf.slab, size, &ppe.ppebuf.slab_mapping);
	if (ret == -1) {
		WARN();
		return 1;
	}

	/* Now we can create the buffer pool */
	pool->pre_alloc_buffer = ppe.ppebuf.slab;

	ret = pool_init(pool, "ppebuf", sizeof(ppebuf_t),
					POOL_PPEBUF, NULL);
	if (ret) {
		WARN();
		return 1;
	}

	return 0;
}

int main(void)
{
	int err;
	int i;

	err = misc_init_once();
	if (err)
		return 1;

	//todo: merge all evl into one func in misc.
	/* Create the event loop thread. */
	evl_init(&evl);

	err = pthread_create(&ppe.event_thread, NULL, event_loop_func, NULL);
	if (unlikely(err)) {
		ptl_warn("event loop creation failed\n");
		return 1;
	}
	ppe.event_thread_run = 1;

	for (i=0; i<0x100; i++)
		INIT_LIST_HEAD(&(ppe.logical_group_list[i]));

	/* Setup the PPE exchange zone. */
	init_ppe();

	err = setup_ppebufs();
	if (err)
		return 1;

	ppe_progress();

	//todo : cleanup (evl, shm, ...)
}

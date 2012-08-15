/**
 * @file p4ppe.c
 *
 * Portals Process Engine main file.
 */

#include "ptl_loc.h"

#include <getopt.h>
#include <sys/un.h>

/* Event loop. */
struct evl evl;

/* Keep global data. */
static struct ppe ppe;

/**
 * Compare two physical NIs belonging to the same node, from their PIDs.
 *
 * @param[in] m1 first mr
 * @param[in] m2 second mr
 *
 * @return -1, 0, or +1 as m1 address is <, == or > m2 address
 */
static int ni_compare(struct ni *ni1, struct ni *ni2)
{
	assert(ni1->options & PTL_NI_PHYSICAL);
	assert(ni2->options & PTL_NI_PHYSICAL);
	assert(ni1->id.phys.nid == ni2->id.phys.nid);

	if (ni2->id.phys.pid != ni1->id.phys.pid)
		return ni2->id.phys.pid - ni1->id.phys.pid;
	else
		return ni2->ni_type - ni1->ni_type;
}

static int ni_set_compare(struct logical_ni_set *set1,
						  struct logical_ni_set *set2)
{
	return(set2->hash - set1->hash);
}

/**
 * Generate RB tree internal functions for the physical NIs, and sets of logical NIs.
 */
RB_GENERATE_STATIC(phys_ni_root, ni, mem.entry, ni_compare);
RB_GENERATE_STATIC(logical_ni_set_root, logical_ni_set, entry, ni_set_compare);

static int  clients_compare(struct client *client1, struct client *client2)
{
	return client2->pid - client1->pid;
}

RB_GENERATE_STATIC(clients_root, client, entry, clients_compare);

/* Given a memory segment, create a mapping for XPMEM, and return the
 * segid and the offset of the buffer. Return PTL_OK on success. */
static int create_mapping_ppe(const void *addr_in, size_t length, struct xpmem_map *mapping)
{
	void *addr;

	/* Align the address to a page boundary. */
	addr = (void *)(((uintptr_t)addr_in) & ~(pagesize-1));
	mapping->offset = addr_in - addr;
	mapping->source_addr = addr_in;

	/* Adjust the size to comprise full pages. */
	mapping->size = length;
	length += mapping->offset;
	length = (length + pagesize-1) & ~(pagesize-1);

	mapping->segid = xpmem_make(addr, length,
								XPMEM_PERMIT_MODE, (void *)0600);

	return mapping->segid == -1 ? PTL_ARG_INVALID : PTL_OK;
}

/* Delete an existing mapping. */
static void delete_mapping_ppe(struct xpmem_map *mapping)
{
	 xpmem_remove(mapping->segid);
}

/* Create a unique ppebuf pool, to be used/shared by all clients. */
static int setup_ppebufs(void)
{
	size_t size;
	int ret;
	pool_t *pool;
	size_t slab_size;

	slab_size = ppe.ppebuf.num * sizeof(ppebuf_t);

	/* Round up to page size. */
	size = sizeof(struct ppe_comm_pad) + slab_size;
	size = ROUND_UP(size, pagesize);

	if (posix_memalign((void **)&ppe.comm_pad, pagesize, size)) {
		WARN();
		return 1;
	}

	ppe.ppebuf.slab = ppe.comm_pad->ppebuf_slab;

	/* Make the whole thing shareable through XPMEM. */
	ret = create_mapping_ppe(ppe.comm_pad, size, &ppe.comm_pad_mapping);
	if (ret == -1) {
		WARN();
		return 1;
	}

	/* Now we can create the buffer pool */
	pool = &ppe.comm_pad->ppebuf_pool;
	pool->pre_alloc_buffer = ppe.ppebuf.slab;
	pool->use_pre_alloc_buffer = 1;
	pool->slab_size = slab_size;

	ret = pool_init(&ppe.gbl, pool, "ppebuf", sizeof(ppebuf_t),
					POOL_PPEBUF, NULL);
	if (ret) {
		WARN();
		return 1;
	}

	return 0;
}

static struct client *find_client(pid_t pid)
{
	struct client find;
	struct client *res;

	find.pid = pid;
	res = RB_FIND(clients_root, &ppe.clients_tree, &find);

	return res;
}

static void destroy_client(struct client *client)
{
	RB_REMOVE(clients_root, &ppe.clients_tree, client);

	ev_io_stop(evl.loop, &client->watcher);

	close(client->s);

	/* If the client has crashed, then we should free all its
	 * ressources. TODO */

	free(client);
}

/* Process a request from a client. */
static void process_client_msg(EV_P_ ev_io *w, int revents)
{
	union msg_ppe_client msg;
	int len;
	struct client *client = w->data;
	struct client *client2;

	len = recv(client->s, &msg, sizeof(msg), 0);
	if (len != sizeof(msg)) {
		destroy_client(client);
		return;
	}

	client2 = find_client(msg.req.pid);
	if (client2) {
		/* If we already have a client with the same PID, the old client
		 * died and we don't know yet. */
		destroy_client(client2);
	}

	client->pid = msg.req.pid;
	client->gbl.apid = xpmem_get(msg.req.segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL);
	if (client->gbl.apid == -1) {
		/* That is possible, but should not happen. */
		msg.rep.ret = PTL_FAIL;
	} 
	else if (index_init(&client->gbl) != PTL_OK) {
		msg.rep.ret = PTL_FAIL;
	} else {
		/* Designate a progress thread for this client. They are alloted
		 * on a round-robin fashion. */
		client->gbl.prog_thread = ppe.current_prog_thread;
		ppe.current_prog_thread++;
		if (ppe.current_prog_thread >= ppe.num_prog_threads)
			ppe.current_prog_thread = 0;

		RB_INSERT(clients_root, &ppe.clients_tree, client);

		msg.rep.cookie = client;
		msg.rep.ppebufs_mapping = ppe.comm_pad_mapping;
		msg.rep.ppebufs_ppeaddr = ppe.comm_pad->ppebuf_slab;
		msg.rep.queue_index = client->gbl.prog_thread;
		msg.rep.ret = PTL_OK;
	}

	if (send(client->s, &msg, sizeof(msg), 0) != sizeof(msg)) {
		/* The client just died. Whatever. */
		destroy_client(client);
	}
}

/* Process an incomig connection request. */
static void process_client_connection(EV_P_ ev_io *w, int revents)
{
	int s;

	s = accept(ppe.client_fd, NULL, NULL);

	if (s != -1) {
		/* Create the new client and add a watcher. */
		struct client *client = calloc(1, sizeof(struct client));
		if (client) {
			client->s = s;

			/* Add a watcher on this client. */
			ev_io_init(&client->watcher, process_client_msg, s, EV_READ);
			client->watcher.data = client;

			ev_io_start(evl.loop, &client->watcher);

			/* TODO: is there a need to keep a client on a pending
			 * connection list? */
		} else {
			/* An error occurred. Reject the client. */
			WARN();
			close(s);
		}
	}
}

/* The communication pad for the PPE is only 4KB, and only contains a
 * queue structure. There is no buffers */
static int init_ppe(void)
{
    struct sockaddr_un sockaddr;
	int err;
	int i;

	/* Initialize the NI trees. */
	RB_INIT(&ppe.physni_tree);
	RB_INIT(&ppe.set_tree);

	RB_INIT(&ppe.clients_tree);

	/* Create the socket on which the client connect to. */
	if ((ppe.client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		WARN();
		goto exit_fail;
    }

    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, PPE_SOCKET_NAME);
    unlink(PPE_SOCKET_NAME);
    if (bind(ppe.client_fd, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_un)) == -1) {
		WARN();
		goto exit_fail;
    }

    if (listen(ppe.client_fd, 50) == -1) {
		WARN();
		goto exit_fail;
    }

	/* Add a watcher for incoming requests. */
	ppe.client_watcher.data = NULL;
	ev_io_init(&ppe.client_watcher, process_client_connection,
		   ppe.client_fd, EV_READ);

	EVL_WATCH(ev_io_start(evl.loop, &ppe.client_watcher));

	/* Create the PPEBUFs. */
	err = setup_ppebufs();
	if (err) {
		WARN();
		goto exit_fail;
	}

	for (i=0; i<ppe.num_prog_threads; i++) {
		struct prog_thread *pt = &ppe.prog_thread[i];

		/* Initialize both thread queues. */
		pt->queue = &ppe.comm_pad->q[i].queue;
		queue_init(pt->queue);
		queue_init(&pt->internal_queue);
	}

	INIT_LIST_HEAD(&ppe.ni_list);

	return PTL_OK;

 exit_fail:
	return PTL_FAIL;
}

static struct logical_ni_set *get_ni_set(unsigned int hash)
{
	struct logical_ni_set find, *res;

	find.hash = hash;
	res = RB_FIND(logical_ni_set_root, &ppe.set_tree, &find);

	return res;
}

/* Find the NI set for an incoming buffer. */
static ni_t *get_dest_ni(buf_t *mem_buf)
{
	struct hdr_common *hdr = mem_buf->data;

	if (hdr->physical) {
		struct ni find, *res;

		find.options = PTL_NI_PHYSICAL;
		find.id.phys.nid = le32_to_cpu(hdr->dst_nid);
		find.id.phys.pid = le32_to_cpu(hdr->dst_pid);
		find.ni_type = hdr->ni_type;

		res = RB_FIND(phys_ni_root, &ppe.physni_tree, &find);

		return res;
	} else {
		struct logical_ni_set *set;
		ni_t *ni;

		set = get_ni_set(le32_to_cpu(hdr->hash));

		if (!set)
			return NULL;

		ni = set->ni[le32_to_cpu(hdr->dst_rank)];
		return ni;
	}
}

/**
 * @param[in] ni
 * @param[in] conn
 *
 * @return status
 *
 * conn must be locked
 */
static int ppe_init_connect(ni_t *ni, conn_t *conn)
{
	/* Q: is that ever called ? */
	return PTL_OK;
}

static int ppe_send_message(buf_t *buf, int from_init)
{
	ni_t *ni;

	/* Keep a reference on the buffer so it doesn't get freed. will be
	 * returned by the remote side with type=BUF_SHMEM_RETURN. */
	assert(buf->obj.obj_pool->type == POOL_BUF);
	buf_get(buf);

	buf->type = BUF_MEM_SEND;
	buf->obj.next = NULL;

	ni = buf->dest_ni = get_dest_ni(buf);

	/* Enqueue on the internal queue attached to that NI. */
	enqueue(NULL, ni->mem.internal_queue, (obj_t *)buf);

	return PTL_OK;
}

static void ppe_set_send_flags(buf_t *buf, int can_inline)
{
	/* The data is always in the buffer. */
	buf->event_mask |= XX_INLINE;
}

static void append_init_data_ppe_direct(data_t *data, mr_t *mr, void *addr,
										ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_MEM_DMA;

	data->mem.num_mem_iovecs = 1;
	data->mem.mem_iovec[0].addr = addr_to_ppe(addr, mr);
	data->mem.mem_iovec[0].length = length;

	buf->length += sizeof(*data) + sizeof(struct mem_iovec);
}

/* Let the remote side know where we store the IOVECS. This is used
 * for both direct and indirect iovecs cases. That avoids a copy into
 * the message buffer. */
static void append_init_data_ppe_iovec(data_t *data, md_t *md, int iov_start,
												int num_iov, ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_MEM_INDIRECT;
	data->mem.num_mem_iovecs = num_iov;

	data->mem.mem_iovec[0].addr = &md->mem_iovecs[iov_start];
	data->mem.mem_iovec[0].length = num_iov * sizeof(struct mem_iovec);

	buf->length += sizeof(*data) + sizeof(struct mem_iovec);
}

/**
 * @brief Build and append a data segment to a request message.
 *
 * @param[in] md the md that contains the data
 * @param[in] dir the data direction, in or out
 * @param[in] offset the offset into the md
 * @param[in] length the length of the data
 * @param[in] buf the buf the add the data segment to
 * @param[in] type the transport type
 *
 * @return status
 */
static int ppe_init_prepare_transfer(md_t *md, data_dir_t dir, ptl_size_t offset,
									 ptl_size_t length, buf_t *buf)
{
	int err = PTL_OK;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *data = (data_t *)(buf->data + buf->length);
	int num_sge;
	ptl_size_t iov_start = 0;
	ptl_size_t iov_offset = 0;

	if (length <= get_param(PTL_MAX_INLINE_DATA)) {
		mr_t *mr;
		if (md->num_iov) {
			err = append_immediate_data(md->start, md->mr_list, md->num_iov, dir, offset, length, buf);
		} else {
			err = mr_lookup_app(obj_to_ni(md), md->start + offset, length, &mr);
			if (err) {
				WARN();
				return PTL_FAIL;
			}

			err = append_immediate_data(md->start, &mr, md->num_iov, dir, offset, length, buf);

			mr_put(mr);
		}
	}
	else if (md->options & PTL_IOVEC) {
		ptl_iovec_t *iovecs = md->start;

		/* Find the index and offset of the first IOV as well as the
		 * total number of IOVs to transfer. */
		num_sge = iov_count_elem(iovecs, md->num_iov,
								 offset, length, &iov_start, &iov_offset);
		if (num_sge < 0) {
			WARN();
			return PTL_FAIL;
		}

		append_init_data_ppe_iovec(data, md, iov_start, num_sge, length, buf);

		/* @todo this is completely bogus */
		/* Adjust the header offset for iov start. */
		hdr->roffset = cpu_to_le64(le64_to_cpu(hdr->roffset) - iov_offset);
	} else {
		void *addr;
		mr_t *mr;
		ni_t *ni = obj_to_ni(md);

		addr = md->start + offset;
		err = mr_lookup_app(ni, addr, length, &mr);
		if (!err) {
			buf->mr_list[buf->num_mr++] = mr;

			append_init_data_ppe_direct(data, mr, addr, length, buf);
		}
	}

	if (!err)
		assert(buf->length <= BUF_DATA_SIZE);

	return err;
}

static int ppe_tgt_data_out(buf_t *buf, data_t *data)
{
	int next;

	switch(data->data_fmt) {
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

	return next;
}

struct transport transport_mem = {
	.type = CONN_TYPE_MEM,
	.buf_alloc = buf_alloc,
	.init_connect = ppe_init_connect,
	.post_tgt_dma = mem_do_transfer,
	.send_message = ppe_send_message,
	.set_send_flags = ppe_set_send_flags,
	.init_prepare_transfer = ppe_init_prepare_transfer,
	.tgt_data_out = ppe_tgt_data_out,
};

/* Return object to client. The PPE cannot access that buf afterwards. */
static inline void buf_completed(ppebuf_t *buf)
{
	buf->completed = 1;
	__sync_synchronize();
}

/* Attach to an XPMEM segment from a given client. */
static int map_segment_ppe(struct client *client, const void *client_addr, size_t len, void **ret)
{
	off_t offset;
    struct xpmem_addr addr;
	void *ptr_attach;

	if (len == 0) {
		/* Nothing to map. It's still a valid call. */
		*ret = NULL;
		return 0;
	}

	/* Hack. When addr.offset is not page aligned, xpmem_attach()
	 * always fail. So fix the ptr afterwards. */
	offset = ((uintptr_t)client_addr) & (pagesize-1);
	addr.offset = (uintptr_t)client_addr - offset;
	addr.apid = client->gbl.apid;

	ptr_attach = xpmem_attach(addr, len+offset, NULL);
	if (ptr_attach == (void *)-1) {
		WARN();
		*ret = NULL;
		return 1;
	}

	*ret = ptr_attach + offset;

	return 0;
}

/* Detach from an XPMEM segment. */
static void unmap_segment_ppe(void *ptr_attach)
{
	/* The pointer given is somewhere in the page (see
	   map_segment_ppe() ). Round it down to the page boundary. */
	if (xpmem_detach((void *)((uintptr_t)ptr_attach & ~(pagesize-1))) != 0) {
		abort();
		WARN();
	}
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlNIStatus(&client->gbl,
								buf->msg.PtlNIStatus.ni_handle,
								buf->msg.PtlNIStatus.status_register,
								&buf->msg.PtlNIStatus.status);
}

/* Remove an NI from a PPE set. */
static void remove_ni(ni_t *ni)
{
	if (ni->options & PTL_NI_PHYSICAL) {
		RB_REMOVE(phys_ni_root, &ppe.physni_tree, ni);
	} else {
		struct logical_ni_set *set;

		if (!ni->mem.in_set)
			return;

		ni->mem.in_set = 0;

		set = get_ni_set(ni->mem.hash);

		assert(set);
		assert(set->ni[ni->id.rank] == ni);

		set->ni[ni->id.rank] = NULL;
		set->members --;
		if (set->members == 0) {
			/* Remove set. */
			RB_REMOVE(logical_ni_set_root, &ppe.set_tree, set);

			free(set->ni);
			free(set);
		}
	}
}

static void do_OP_PtlNIFini(ppebuf_t *buf)
{
	int err;
	struct client *client = buf->cookie;
	ni_t *ni;

	err = to_ni(&client->gbl, buf->msg.PtlNIFini.ni_handle, &ni);
	if (unlikely(err)) {
		WARN();
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}

	buf->msg.ret = _PtlNIFini(&client->gbl,
							  buf->msg.PtlNIFini.ni_handle);

	if (atomic_read(&ni->ref_cnt) == 0) {
		remove_ni(ni);
#if WITH_TRANSPORT_IB
		list_del(&ni->rdma.ppe_ni_list);
#endif
	}

	ni_put(ni);

	if (buf->msg.ret)
		WARN();
}

static void do_OP_PtlNIHandle(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlNIHandle(&client->gbl,
								buf->msg.PtlNIHandle.handle,
								&buf->msg.PtlNIHandle.ni_handle);
}

static void insert_ni_into_set(gbl_t *gbl, ni_t *ni)
{
	if (ni->options & PTL_NI_PHYSICAL) {
		void *res;

		/* Insert the NI into the RB tree. */

		res = RB_INSERT(phys_ni_root, &ppe.physni_tree, ni);
		assert(res == NULL);	/* should never happen */
	} else {
		struct logical_ni_set *set;

		/* Insert the NI into the hash table. */

		set = get_ni_set(ni->mem.hash);

		if (set == NULL) {
			//todo: should set be a pool?
			set = calloc(1, sizeof(*set));

			set->hash = ni->mem.hash;
			set->ni = calloc(sizeof (ni_t *), ni->logical.map_size);

			RB_INSERT(logical_ni_set_root, &ppe.set_tree, set);
		} else {
			/* The set already exists. */
			if (set->ni[ni->id.rank]) {
				/* Error. There is something there already. Cannot
				 * happen, unless there is a hash collision. Hard to
				 * recover from. */
				abort();
			}
		}

		set->ni[ni->id.rank] = ni;
		set->members ++;

		ni->mem.in_set = 1;
	}
}

static void do_OP_PtlSetMap(ppebuf_t *buf)
{
	struct client *client = buf->cookie;
	ptl_process_t *mapping;
	int ret;

	ret = map_segment_ppe(client, buf->msg.PtlSetMap.mapping,
						  buf->msg.PtlSetMap.map_size*sizeof(ptl_process_t),
						  (void **)&mapping);
	if (!ret) {
		ret = _PtlSetMap(&client->gbl,
						 buf->msg.PtlSetMap.ni_handle,
						 buf->msg.PtlSetMap.map_size,
						 mapping);

		buf->msg.ret = ret;

		if (ret == PTL_OK) {
			ni_t *ni;

			/* retrieve the NI. This cannot fail because PtlSetMap succedeed. */
			ret = to_ni(&client->gbl, buf->msg.PtlSetMap.ni_handle, &ni);
			assert(ret == PTL_OK);

			insert_ni_into_set(&client->gbl, ni);
		}

		unmap_segment_ppe(mapping);
	} else {
		WARN();
		buf->msg.ret = PTL_ARG_INVALID;
	}
}

static void do_OP_PtlGetMap(ppebuf_t *buf)
{
	struct client *client = buf->cookie;
	ptl_process_t *mapping;
	int ret;

	ret = map_segment_ppe(client, buf->msg.PtlGetMap.mapping,
						  buf->msg.PtlGetMap.map_size*sizeof(ptl_process_t),
						  (void **)&mapping);
	if (!ret) {
		buf->msg.ret = _PtlGetMap(&client->gbl,
								  buf->msg.PtlGetMap.ni_handle,
								  buf->msg.PtlGetMap.map_size,
								  mapping,
								  &buf->msg.PtlGetMap.actual_map_size);

		unmap_segment_ppe(mapping);
	} else {
		buf->msg.ret = PTL_ARG_INVALID;
	}
}

static void do_OP_PtlPTAlloc(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlPTAlloc(&client->gbl,
							   buf->msg.PtlPTAlloc.ni_handle,
							   buf->msg.PtlPTAlloc.options,
							   buf->msg.PtlPTAlloc.eq_handle,
							   buf->msg.PtlPTAlloc.pt_index_req,
							   &buf->msg.PtlPTAlloc.pt_index);
}

static void do_OP_PtlPTFree(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlPTFree(&client->gbl,
							  buf->msg.PtlPTFree.ni_handle,
							  buf->msg.PtlPTFree.pt_index);
}

static void do_OP_PtlMESearch(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlMESearch(&client->gbl,
								buf->msg.PtlMESearch.ni_handle,
								buf->msg.PtlMESearch.pt_index,
								&buf->msg.PtlMESearch.me,
								buf->msg.PtlMESearch.ptl_search_op,
								buf->msg.PtlMESearch.user_ptr);
}

static void do_OP_PtlMEAppend(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlMEAppend(&client->gbl,
								buf->msg.PtlMEAppend.ni_handle,
								buf->msg.PtlMEAppend.pt_index,
								&buf->msg.PtlMEAppend.me,
								buf->msg.PtlMEAppend.ptl_list,
								buf->msg.PtlMEAppend.user_ptr,
								&buf->msg.PtlMEAppend.me_handle);
}

static void do_OP_PtlMEUnlink(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlMEUnlink(&client->gbl,
								buf->msg.PtlMEUnlink.me_handle);
}

static void do_OP_PtlLEAppend(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlLEAppend(&client->gbl,
								buf->msg.PtlLEAppend.ni_handle,
								buf->msg.PtlLEAppend.pt_index,
								&buf->msg.PtlLEAppend.le,
								buf->msg.PtlLEAppend.ptl_list,
								buf->msg.PtlLEAppend.user_ptr,
								&buf->msg.PtlLEAppend.le_handle);
}

static void do_OP_PtlLESearch(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlLESearch(&client->gbl,
								buf->msg.PtlLESearch.ni_handle,
								buf->msg.PtlLESearch.pt_index,
								&buf->msg.PtlLESearch.le,
								buf->msg.PtlLESearch.ptl_search_op,
								buf->msg.PtlLESearch.user_ptr);
}

static void do_OP_PtlLEUnlink(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlLEUnlink(&client->gbl,
								buf->msg.PtlLEUnlink.le_handle);
}

static void do_OP_PtlMDBind(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlMDBind(&client->gbl,
							  buf->msg.PtlMDBind.ni_handle,
							  &buf->msg.PtlMDBind.md,
							  &buf->msg.PtlMDBind.md_handle);
}

static void do_OP_PtlMDRelease(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlMDRelease(&client->gbl,
								 buf->msg.PtlMDRelease.md_handle);
}

static void do_OP_PtlGetId(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlGetId(&client->gbl,
							 buf->msg.PtlGetId.ni_handle,
							 &buf->msg.PtlGetId.id);
}

static void do_OP_PtlPut(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlPut(&client->gbl,
						   buf->msg.PtlPut.md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlGet(&client->gbl,
						   buf->msg.PtlGet.md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlAtomic(&client->gbl,
							  buf->msg.PtlAtomic.md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlFetchAtomic(&client->gbl,
								   buf->msg.PtlFetchAtomic.get_md_handle,
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
	void *operand;
	struct client *client = buf->cookie;
	int ret;

	ret = map_segment_ppe(client, buf->msg.PtlSwap.operand,
						  buf->msg.PtlSwap.length,
						  (void **)&operand);
	if (!ret) {
		buf->msg.ret = _PtlSwap(&client->gbl,
								buf->msg.PtlSwap.get_md_handle,
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
								operand,
								buf->msg.PtlSwap.operation,
								buf->msg.PtlSwap.datatype);
		unmap_segment_ppe(operand);
	} else {
		buf->msg.ret = PTL_ARG_INVALID;
	}
}

static void do_OP_PtlTriggeredPut(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlTriggeredPut(&client->gbl,
									buf->msg.PtlTriggeredPut.md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlTriggeredGet(&client->gbl,
									buf->msg.PtlTriggeredGet.md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlTriggeredAtomic(&client->gbl,
									   buf->msg.PtlTriggeredAtomic.md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlTriggeredFetchAtomic(&client->gbl,
											buf->msg.PtlTriggeredFetchAtomic.get_md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlTriggeredSwap(&client->gbl,
									 buf->msg.PtlTriggeredSwap.get_md_handle,
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
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlTriggeredCTInc(&client->gbl,
									  buf->msg.PtlTriggeredCTInc.ct_handle,
									  buf->msg.PtlTriggeredCTInc.increment,
									  buf->msg.PtlTriggeredCTInc.trig_ct_handle,
									  buf->msg.PtlTriggeredCTInc.threshold);
}

static void do_OP_PtlTriggeredCTSet(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlTriggeredCTSet(&client->gbl,
									  buf->msg.PtlTriggeredCTSet.ct_handle,
									  buf->msg.PtlTriggeredCTSet.new_ct,
									  buf->msg.PtlTriggeredCTSet.trig_ct_handle,
									  buf->msg.PtlTriggeredCTSet.threshold);
}

static void do_OP_PtlGetPhysId(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlGetPhysId(&client->gbl,
								 buf->msg.PtlGetPhysId.ni_handle,
								 &buf->msg.PtlGetPhysId.id);
}

static void do_OP_PtlEQAlloc(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlEQAlloc(&client->gbl,
							   buf->msg.PtlEQAlloc.ni_handle,
							   buf->msg.PtlEQAlloc.count,
							   &buf->msg.PtlEQAlloc.eq_handle);

	if (buf->msg.ret == PTL_OK) {
		int err;
		eq_t *eq;

		/* Should not fail since it was just created. */
		err = to_eq(&client->gbl, buf->msg.PtlEQAlloc.eq_handle, &eq);
		assert(err == PTL_OK);

		err = create_mapping_ppe(eq->eqe_list, eq->eqe_list_size,
								 &eq->ppe.eqe_list);
		buf->msg.PtlEQAlloc.eqe_list = eq->ppe.eqe_list;

		eq_put(eq);

		if (err != PTL_OK) {
			_PtlEQFree(&client->gbl,
					   buf->msg.PtlEQAlloc.eq_handle);
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
	struct client *client = buf->cookie;

	err = to_eq(&client->gbl, buf->msg.PtlEQFree.eq_handle, &eq);
	if (err || !eq) {
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}
	mapping = eq->ppe.eqe_list;
	eq_put(eq);

	buf->msg.ret = _PtlEQFree(&client->gbl,
							  buf->msg.PtlEQFree.eq_handle);

	if (buf->msg.ret == PTL_OK)
		delete_mapping_ppe(&mapping);
}

static void do_OP_PtlCTAlloc(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlCTAlloc(&client->gbl,
							   buf->msg.PtlCTAlloc.ni_handle,
							   &buf->msg.PtlCTAlloc.ct_handle);

	if (buf->msg.ret == PTL_OK) {
		int err;
		ct_t *ct;

		/* Should not fail since it was just created. */
		err = to_ct(&client->gbl, buf->msg.PtlCTAlloc.ct_handle, &ct);
		assert(err == PTL_OK);

		err = create_mapping_ppe(&ct->info, sizeof(struct ct_info),
								 &ct->ppe.ct_mapping);
		buf->msg.PtlCTAlloc.ct_mapping = ct->ppe.ct_mapping;

		ct_put(ct);

		if (err != PTL_OK) {
			_PtlCTFree(&client->gbl,
					   buf->msg.PtlCTAlloc.ct_handle);
			buf->msg.ret = PTL_ARG_INVALID;
			return;
		}
	}
}

static void do_OP_PtlCTInc(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlCTInc(&client->gbl,
							 buf->msg.PtlCTInc.ct_handle, buf->msg.PtlCTInc.increment);
}

static void do_OP_PtlCTSet(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlCTSet(&client->gbl,
							 buf->msg.PtlCTSet.ct_handle, buf->msg.PtlCTSet.new_ct);
}

static void do_OP_PtlCTFree(ppebuf_t *buf)
{
	int err;
	ct_t *ct;
	struct xpmem_map mapping;
	struct client *client = buf->cookie;

	err = to_ct(&client->gbl, buf->msg.PtlCTFree.ct_handle, &ct);
	if (err || !ct) {
		buf->msg.ret = PTL_ARG_INVALID;
		return;
	}
	mapping = ct->ppe.ct_mapping;
	ct_put(ct);

	buf->msg.ret = _PtlCTFree(&client->gbl,
							  buf->msg.PtlCTFree.ct_handle);

	if (buf->msg.ret == PTL_OK)
		delete_mapping_ppe(&mapping);
}

static void do_OP_PtlPTDisable(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlPTDisable(&client->gbl,
								 buf->msg.PtlPTDisable.ni_handle,
								 buf->msg.PtlPTDisable.pt_index);
}

static void do_OP_PtlPTEnable(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlPTEnable(&client->gbl,
								buf->msg.PtlPTDisable.ni_handle,
								buf->msg.PtlPTDisable.pt_index);
}

static void do_OP_PtlAtomicSync(ppebuf_t *buf)
{
	buf->msg.ret = PtlAtomicSync();
}

static void do_OP_PtlCTCancelTriggered(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlCTCancelTriggered(&client->gbl,
										 buf->msg.PtlCTCancelTriggered.ct_handle);
}

static void do_OP_PtlGetUid(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlGetUid(&client->gbl,
							  buf->msg.PtlGetUid.ni_handle,
							  &buf->msg.PtlGetUid.uid);
}

static void do_OP_PtlStartBundle(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlStartBundle(&client->gbl,
								   buf->msg.PtlStartBundle.ni_handle);
}

static void do_OP_PtlEndBundle(ppebuf_t *buf)
{
	struct client *client = buf->cookie;

	buf->msg.ret = _PtlEndBundle(&client->gbl,
								 buf->msg.PtlEndBundle.ni_handle);
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
	ADD_OP(PtlStartBundle),
	ADD_OP(PtlEndBundle),
};

/* Progress thread for the PPE. */
static void *ppe_progress(void *arg)
{
	struct prog_thread *pt = arg;

	while (!pt->stop) {
		ppebuf_t *ppebuf;
		buf_t *mem_buf;

#if WITH_TRANSPORT_IB
		/* Infiniband. Walking the list of active NIs to find work. */
		ni_t *ni;
		list_for_each_entry(ni, &ppe.ni_list, rdma.ppe_ni_list) {
			progress_thread_rdma(ni);
		}
#endif

		/* Get message from the message queue. */
		ppebuf = (ppebuf_t *)dequeue(NULL, pt->queue);

		if (ppebuf) {
			ppe_ops[ppebuf->op].func(ppebuf);

			/* Return response to blocked client. */
			buf_completed(ppebuf);
		}

		/* Get message from our own queue. */
		mem_buf = (buf_t *)dequeue(NULL, &pt->internal_queue);
		if (mem_buf) {
			int err;

			if (mem_buf->type == BUF_MEM_SEND) {
				buf_t *buf;
				ni_t *ni;

				/* Mark it for releasing. The target state machine might
				 * change its type back to BUF_MEM_SEND. */
				mem_buf->type = BUF_MEM_RELEASE;

				/* The destination NI has been computed by send_message_mem. */
				ni = mem_buf->dest_ni;
				err = buf_alloc(ni, &buf);
				if (err) {
					WARN();
				} else {
					buf->data = mem_buf->internal_data;
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

	return NULL;
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
	err = pool_init(gbl, &gbl->ni_pool, "ni", sizeof(ni_t), POOL_NI, NULL);
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
static int NIInit_ppe(gbl_t *gbl, ni_t *ni)
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

	ni->mem.internal_queue = &ppe.prog_thread[gbl->prog_thread].internal_queue;
	ni->mem.apid = gbl->apid;

#if WITH_TRANSPORT_IB
	list_add_tail(&ni->rdma.ppe_ni_list, &ppe.ni_list);
#endif

	if (ni->options & PTL_NI_PHYSICAL) {
		/* Physical interface. We are connected to ourselves. */
		conn_t *conn = get_conn(ni, ni->id);

		if (!conn) {
			/* It's hard to recover from here. */
			WARN();
			abort();
		}

		conn->transport = transport_mem;
		conn->state = CONN_STATE_CONNECTED;

		conn_put(conn);			/* from get_conn */

		insert_ni_into_set(gbl, ni);
	}

	return PTL_OK;
}

struct transport_ops transport_local_ppe = {
	.NIInit = NIInit_ppe,
	.SetMap = PtlSetMap_mem,
};

static void stop_event_loop_func(EV_P_ ev_async *w, int revents)
{
	ev_break(evl.loop, EVBREAK_ALL);
}

static void sig_terminate(int signum)
{
	/* Create an async event to stop the event loop. May be there
	 * is a better way. */
	ev_async stop_event_loop;
	ev_async_init(&stop_event_loop, stop_event_loop_func);
	EVL_WATCH(ev_async_start(evl.loop, &stop_event_loop));
	ev_async_send(evl.loop, &stop_event_loop);
}

int main(int argc, char *argv[])
{
	int err;
	int i;
	int c;

	/* Set some default values. */
	ppe.ppebuf.num = 1000;
	ppe.num_prog_threads = 1;

	while (1) {
		int option_index;
		static struct option long_options[] = {
			{"nppebufs", 1, 0, 'n'},
			{"nprogthreads", 1, 0, 'p'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "n:p:",
                        long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'n':
			ppe.ppebuf.num = atoi(optarg);
			if (ppe.ppebuf.num < 1) {
				ptl_warn("Invalid argument value for nppebufs\n");
				return 1;
			}
			break;

		case 'p':
			ppe.num_prog_threads = atoi(optarg);
			if (ppe.num_prog_threads < 1 || ppe.num_prog_threads>MAX_PROGRESS_THREADS) {
				ptl_warn("Invalid argument value for nprogthreads\n");
				return 1;
			}
			break;

		default:
			ptl_warn("Invalid option %s\n", argv[option_index]);
			return 1;
		}
	}

	/* Misc initializations. */
	err = misc_init_once();
	if (err)
		return 1;

	/* Init the index service */
	err = index_init(&ppe.gbl);
	if (err)
		return err;

	/* Create the event loop thread. */
	evl_init(&evl);

	/* Setup the PPE. */
	err = init_ppe();
	if (err)
		return 1;

	/* Launch the threads. */
	for (i=0; i<ppe.num_prog_threads; i++) {
		struct prog_thread *pt = &ppe.prog_thread[i];
		err = pthread_create(&pt->thread, NULL, ppe_progress, pt);
		if (unlikely(err)) {
			ptl_warn("Failed to create a progress thread.\n");
			return 1;
		}
	}

	signal(SIGTERM, sig_terminate);

	/* Start the event loop. Does not exit. */
	evl_run(&evl);

	//todo: on shutdown, we might want to cleanup
}

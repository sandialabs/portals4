/**
 * @file ptl_shmem.c
 */

#include "ptl_loc.h"

/**
 * @brief Send a message using shared memory.
 *
 * @param[in] buf
 * @param[in] signaled
 *
 * @return status
 */
static int send_message_shmem(buf_t *buf, int from_init)
{
	/* Keep a reference on the buffer so it doesn't get freed. will be
	 * returned by the remote side with type=BUF_SHMEM_RETURN. */
	assert(buf->obj.obj_pool->type == POOL_SBUF);
	buf_get(buf);

	buf->type = BUF_SHMEM_SEND;

	buf->shmem.index_owner = buf->obj.obj_ni->mem.index;

	shmem_enqueue(buf->obj.obj_ni, buf,
				buf->dest.shmem.local_rank);

	return PTL_OK;
}

static void shmem_set_send_flags(buf_t *buf, int can_signal)
{
	/* The data is always in the buffer. */
	buf->event_mask |= XX_INLINE;
}

#if USE_KNEM
static void append_init_data_shmem_direct(data_t *data, mr_t *mr, void *addr,
										  ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_KNEM_DMA;
	data->mem.num_mem_iovecs = 1;
	data->mem.mem_iovec[0].cookie = mr->knem_cookie;
	data->mem.mem_iovec[0].offset = addr - mr->addr;
	data->mem.mem_iovec[0].length = length;

	buf->length += sizeof(*data) + sizeof(struct mem_iovec);
}

static void append_init_data_shmem_iovec_direct(data_t *data, md_t *md,
												int iov_start, int num_iov,
												ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_KNEM_DMA;
	data->mem.num_mem_iovecs = num_iov;
	memcpy(data->mem.mem_iovec,
		   &md->mem_iovecs[iov_start],
		   num_iov*sizeof(struct mem_iovec));

	buf->length += sizeof(*data) + num_iov * sizeof(struct mem_iovec);
}

static void append_init_data_shmem_iovec_indirect(data_t *data, md_t *md,
												  int iov_start, int num_iov,
												  ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_KNEM_INDIRECT;
	data->mem.num_mem_iovecs = num_iov;

	data->mem.mem_iovec[0].cookie
		= md->sge_list_mr->knem_cookie;
   //TODO: BUG ALERT? iov_start not used - should affect offset!
	data->mem.mem_iovec[0].offset
		= (void *)md->mem_iovecs - md->sge_list_mr->addr;
	data->mem.mem_iovec[0].length
		= num_iov * sizeof(struct mem_iovec);

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
static int init_prepare_transfer_shmem(md_t *md, data_dir_t dir, ptl_size_t offset,
									   ptl_size_t length, buf_t *buf)
{
	int err = PTL_OK;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *data = (data_t *)(buf->data + buf->length);
	int num_sge;
	ptl_size_t iov_start = 0;
	ptl_size_t iov_offset = 0;

	if (length <= get_param(PTL_MAX_INLINE_DATA)) {
		err = append_immediate_data(md->start, NULL, md->num_iov, dir, offset, length, buf);
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

		if (num_sge > get_param(PTL_MAX_INLINE_SGE))
			/* Indirect case. The IOVs do not fit in a buf_t. */
			append_init_data_shmem_iovec_indirect(data, md, iov_start, num_sge, length, buf);
		else
			append_init_data_shmem_iovec_direct(data, md, iov_start, num_sge, length, buf);

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

			append_init_data_shmem_direct(data, mr, addr, length, buf);
		}
	}

	if (!err)
		assert(buf->length <= BUF_DATA_SIZE);

	return err;
}

static int knem_tgt_data_out(buf_t *buf, data_t *data)
{
	int next;

	switch(data->data_fmt) {
	case DATA_FMT_KNEM_DMA:
		buf->transfer.mem.cur_rem_iovec = &data->mem.mem_iovec[0];
		buf->transfer.mem.num_rem_iovecs = data->mem.num_mem_iovecs;
		buf->transfer.mem.cur_rem_off = 0;

		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_KNEM_INDIRECT:
		next = STATE_TGT_SHMEM_DESC;
		break;

	default:
		assert(0);
		WARN();
		next = STATE_TGT_ERROR;
	}

	return next;
}

#else
static void attach_bounce_buffer(buf_t *buf, data_t *data)
{
	void *bb;
	ni_t *ni = obj_to_ni(buf);

	while ((bb = ll_dequeue_obj_alien(&ni->shmem.bounce_buf.head->free_list,
										ni->shmem.bounce_buf.head,
										ni->shmem.bounce_buf.head->head_index0)) == NULL)
		SPINLOCK_BODY();

	buf->transfer.noknem.data = bb;
	buf->transfer.noknem.data_length = ni->shmem.bounce_buf.buf_size;
	buf->transfer.noknem.bounce_offset = bb - (void *)ni->shmem.bounce_buf.head;

	data->noknem.bounce_offset = buf->transfer.noknem.bounce_offset;
}

static void append_init_data_noknem_iovec(data_t *data, md_t *md,
										  int iov_start, int num_iov,
										  ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_NOKNEM;

	data->noknem.target_done = 0;
	data->noknem.init_done = 0;

	buf->transfer.noknem.transfer_state_expected = 0; /* always the initiator here */
	buf->transfer.noknem.noknem = &data->noknem;

	attach_bounce_buffer(buf, data);

	buf->transfer.noknem.num_iovecs = num_iov;
	buf->transfer.noknem.iovecs = &((ptl_iovec_t *)md->start)[iov_start];
	buf->transfer.noknem.offset = 0;

	buf->transfer.noknem.length_left = length;

	buf->length += sizeof(*data);
}

static void append_init_data_noknem_direct(data_t *data, mr_t *mr, void *addr,
										   ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_NOKNEM;

	data->noknem.target_done = 0;
	data->noknem.init_done = 0;

	buf->transfer.noknem.transfer_state_expected = 0; /* always the initiator here */
	buf->transfer.noknem.noknem = &data->noknem;

	attach_bounce_buffer(buf, data);

	/* Describes local memory */
	buf->transfer.noknem.my_iovec.iov_base = addr;
	buf->transfer.noknem.my_iovec.iov_len = length;

	buf->transfer.noknem.num_iovecs = 1;
	buf->transfer.noknem.iovecs = &buf->transfer.noknem.my_iovec;
	buf->transfer.noknem.offset = 0;

	buf->transfer.noknem.length_left = length;

	buf->length += sizeof(*data);
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
static int init_prepare_transfer_noknem(md_t *md, data_dir_t dir, ptl_size_t offset,
										ptl_size_t length, buf_t *buf)
{
	int err = PTL_OK;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *data = (data_t *)(buf->data + buf->length);
	int num_sge;
	ptl_size_t iov_start = 0;
	ptl_size_t iov_offset = 0;

	if (length <= get_param(PTL_MAX_INLINE_DATA)) {
		err = append_immediate_data(md->start, NULL, md->num_iov, dir, offset, length, buf);
	}
	else {
		if (dir == DATA_DIR_IN)
			buf->data_in->noknem.state = 2;
		else
			buf->data_out->noknem.state = 0;

		if (md->options & PTL_IOVEC) {
			ptl_iovec_t *iovecs = md->start;

			/* Find the index and offset of the first IOV as well as the
			 * total number of IOVs to transfer. */
			num_sge = iov_count_elem(iovecs, md->num_iov,
									 offset, length, &iov_start, &iov_offset);
			if (num_sge < 0) {
				WARN();
				return PTL_FAIL;
			}

			append_init_data_noknem_iovec(data, md, iov_start, num_sge, length, buf);

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

				append_init_data_noknem_direct(data, mr, addr, length, buf);
			}
		}
	}

	if (!err)
		assert(buf->length <= BUF_DATA_SIZE);

	return err;
}

static int do_noknem_transfer(buf_t *buf)
{
	struct noknem *noknem = buf->transfer.noknem.noknem;
	ptl_size_t *resid = buf->rdma_dir == DATA_DIR_IN ?
		&buf->put_resid : &buf->get_resid;
	ptl_size_t to_copy;
	int err;

	if (noknem->state != 2)
		return PTL_OK;

	if (noknem->init_done) {
		assert(noknem->target_done);
		return PTL_OK;
	}

	noknem->state = 3;

	if (*resid) {
		if (buf->rdma_dir == DATA_DIR_IN) {
			to_copy = noknem->length;
			if (to_copy > *resid)
				to_copy = *resid;

			err = iov_copy_in(buf->transfer.noknem.data, buf->transfer.noknem.iovecs,
							  NULL,
							  buf->transfer.noknem.num_iovecs,
							  buf->transfer.noknem.offset, to_copy);
		} else {
			to_copy = buf->transfer.noknem.data_length;
			if (to_copy > *resid)
				to_copy = *resid;

			err = iov_copy_out(buf->transfer.noknem.data, buf->transfer.noknem.iovecs,
							   NULL,
							   buf->transfer.noknem.num_iovecs,
							   buf->transfer.noknem.offset, to_copy);

			noknem->length = to_copy;
		}

		/* That should never happen since all lengths were properly
		 * computed before entering. */
		assert(err == PTL_OK);

	} else {
		/* Dropped case. Nothing to transfer, but the buffer must
		 * still be returned. */
		to_copy = 0;
		err = PTL_OK;
	}

	buf->transfer.noknem.offset += to_copy;
	*resid -= to_copy;

	if (*resid == 0)
		noknem->target_done = 1;

	/* Tell the initiator the buffer is his again. */
	__sync_synchronize();
	noknem->state = 0;

	return err;
}

static int noknem_tgt_data_out(buf_t *buf, data_t *data)
{
	int next;
	ni_t *ni = obj_to_ni(buf);

	if (data->data_fmt != DATA_FMT_NOKNEM) {
		assert(0);
		WARN();
		next = STATE_TGT_ERROR;
	}

	buf->transfer.noknem.transfer_state_expected = 2; /* always the target here */
	buf->transfer.noknem.noknem = &data->noknem;

	if ((buf->rdma_dir == DATA_DIR_IN && buf->put_resid) ||
		(buf->rdma_dir == DATA_DIR_OUT && buf->get_resid)) {
		if (buf->me->options & PTL_IOVEC) {
			buf->transfer.noknem.num_iovecs = buf->me->length;
			buf->transfer.noknem.iovecs = buf->me->start;
		} else {
			buf->transfer.noknem.num_iovecs = 1;
			buf->transfer.noknem.iovecs = &buf->transfer.noknem.my_iovec;

			buf->transfer.noknem.my_iovec.iov_base = buf->me->start;
			buf->transfer.noknem.my_iovec.iov_len = buf->me->length;
		}
	}

	buf->transfer.noknem.offset = buf->moffset;
	buf->transfer.noknem.length_left = buf->get_resid;
	buf->transfer.noknem.data = (void *)ni->shmem.bounce_buf.head + data->noknem.bounce_offset;
	buf->transfer.noknem.data_length = ni->shmem.bounce_buf.buf_size;

	return STATE_TGT_START_COPY;
}
#endif

struct transport transport_shmem = {
	.type = CONN_TYPE_SHMEM,
	.buf_alloc = sbuf_alloc,
	.send_message = send_message_shmem,
	.set_send_flags = shmem_set_send_flags,
#if USE_KNEM
	.init_prepare_transfer = init_prepare_transfer_shmem,
	.post_tgt_dma = do_mem_transfer,
	.tgt_data_out = knem_tgt_data_out,
#else
	.init_prepare_transfer = init_prepare_transfer_noknem,
	.post_tgt_dma = do_noknem_transfer,
	.tgt_data_out = noknem_tgt_data_out,
#endif
};

/**
 * @brief Cleanup shared memory resources.
 *
 * @param[in] ni
 */
static void release_shmem_resources(ni_t *ni)
{
	pool_fini(&ni->sbuf_pool);

	if (ni->shmem.comm_pad != MAP_FAILED) {
		munmap(ni->shmem.comm_pad, ni->shmem.comm_pad_size);
		ni->shmem.comm_pad = MAP_FAILED;
	}

	if (ni->shmem.comm_pad_shm_name) {
		/* Destroy the mmaped file so it doesn't pollute.
		 * All ranks try it in case rank 0 died. */
		shm_unlink(ni->shmem.comm_pad_shm_name);

		free(ni->shmem.comm_pad_shm_name);
		ni->shmem.comm_pad_shm_name = NULL;
	}

	knem_fini(ni);
}

/**
 * @brief Initialize shared memory resources.
 *
 * @param[in] ni
 *
 * @return status
 */
int PtlNIInit_shmem(ni_t *ni)
{
	ni->shmem.knem_fd = -1;
	ni->shmem.comm_pad = MAP_FAILED;

	/* Only if IB hasn't setup the NID first. */
	if (ni->iface->id.phys.nid == PTL_NID_ANY) {
		ni->iface->id.phys.nid = 0;
	}
	if (ni->iface->id.phys.pid == PTL_PID_ANY)
		ni->iface->id.phys.pid = getpid();

	ni->id.phys.nid = ni->iface->id.phys.nid;

	if (ni->id.phys.pid == PTL_PID_ANY)
		ni->id.phys.pid = ni->iface->id.phys.pid;

	if (ni->options & PTL_NI_PHYSICAL) {
		/* Used later to setup the buffers. */
		ni->mem.index = 0;
		ni->mem.node_size = 1;
	}

	return PTL_OK;
}

/**
 * @brief Initialize shared memory resources.
 *
 * This function is called during NI creation if the NI is physical,
 * or after PtlSetMap if it is logical.
 *
 * @param[in] ni
 *
 * @return status
 */
int setup_shmem(ni_t *ni)
{
	int shm_fd = -1;
	char comm_pad_shm_name[200] = "";
	int err;
	int i;
	int pid_table_size;
	off_t first_queue_offset;

	/*
	 * Buffers in shared memory. The buffers will be allocated later,
	 * but not by the pool management. We compute the sizes now.
	 */
	/* Allocate a pool of buffers in the mmapped region. */
	ni->shmem.per_proc_comm_buf_numbers = get_param(PTL_NUM_SBUF);

	ni->sbuf_pool.setup = buf_setup;
	ni->sbuf_pool.init = buf_init;
	ni->sbuf_pool.fini = buf_fini;
	ni->sbuf_pool.cleanup = buf_cleanup;
	ni->sbuf_pool.use_pre_alloc_buffer = 1;
	ni->sbuf_pool.round_size = real_buf_t_size();
	ni->sbuf_pool.slab_size = ni->shmem.per_proc_comm_buf_numbers *
		ni->sbuf_pool.round_size;

	/* Open KNEM device */
	if (knem_init(ni)) {
		WARN();
		goto exit_fail;
	}

	if (ni->options & PTL_NI_PHYSICAL) {
		/* Create a unique name for the shared memory file. */
		snprintf(comm_pad_shm_name, sizeof(comm_pad_shm_name),
				 "/portals4-shmem-pid%d-%d",
				 ni->id.phys.pid, ni->options);
	} else {
		/* Create a unique name for the shared memory file. Use the hash
		 * created from the mapping. */
		snprintf(comm_pad_shm_name, sizeof(comm_pad_shm_name),
				 "/portals4-shmem-%x-%d", ni->mem.hash, ni->options);
	}
	ni->shmem.comm_pad_shm_name = strdup(comm_pad_shm_name);

	/* Allocate a pool of buffers in the mmapped region. */
	ni->shmem.per_proc_comm_buf_size =
		sizeof(queue_t) +
		ni->sbuf_pool.slab_size;

	pid_table_size = ni->mem.node_size * sizeof(struct shmem_pid_table);
	pid_table_size = ROUND_UP(pid_table_size, pagesize);

	ni->shmem.comm_pad_size = pid_table_size;

	first_queue_offset = ni->shmem.comm_pad_size;
	ni->shmem.comm_pad_size += (ni->shmem.per_proc_comm_buf_size * ni->mem.node_size);

#if !USE_KNEM
	off_t bounce_buf_offset;
	off_t bounce_head_offset;

	bounce_head_offset = ni->shmem.comm_pad_size;
	ni->shmem.comm_pad_size += ROUND_UP(sizeof(struct shmem_bounce_head), pagesize);

	ni->shmem.bounce_buf.buf_size = get_param(PTL_BOUNCE_BUF_SIZE);
	ni->shmem.bounce_buf.num_bufs = get_param(PTL_BOUNCE_NUM_BUFS);

	bounce_buf_offset = ni->shmem.comm_pad_size;
	ni->shmem.comm_pad_size += ni->shmem.bounce_buf.buf_size * ni->shmem.bounce_buf.num_bufs;
#endif

	/* Open the communication pad. Let rank 0 create the shared memory. */
	assert(ni->shmem.comm_pad == MAP_FAILED);

	if (ni->mem.index == 0) {
		/* Just in case, remove that file if it already exist. */
		shm_unlink(comm_pad_shm_name);

		shm_fd = shm_open(comm_pad_shm_name,
						  O_RDWR | O_CREAT | O_EXCL,
						  S_IRUSR | S_IWUSR);
		assert(shm_fd >= 0);

		if (shm_fd < 0) {
			ptl_warn("shm_open of %s failed (errno=%d)",
					 comm_pad_shm_name, errno);
			goto exit_fail;
		}

		/* Enlarge the memory zone to the size we need. */
		if (ftruncate(shm_fd, ni->shmem.comm_pad_size) != 0) {
			ptl_warn("share memory ftruncate failed");
			shm_unlink(comm_pad_shm_name);
			goto exit_fail;
		}
	} else {
		int try_count;

		/* Try for 10 seconds. That should leave enough time for rank
		 * 0 to create the file. */
		try_count = 100;
		do {
			shm_fd = shm_open(comm_pad_shm_name, O_RDWR,
							  S_IRUSR | S_IWUSR);

			if (shm_fd != -1)
				break;

			usleep(100000);		/* 100ms */
			try_count --;
		} while(try_count);

		if (shm_fd == -1) {
			ptl_warn("Couldn't open the shared memory file %s\n", comm_pad_shm_name);
			goto exit_fail;
		}

		/* Wait for the file to have the right size before mmaping
		 * it. */
		try_count = 100;
		do {
			struct stat buf;

			if (fstat(shm_fd, &buf) == -1) {
				ptl_warn("Couldn't fstat the shared memory file\n");
				goto exit_fail;
			}

			if (buf.st_size >= ni->shmem.comm_pad_size)
				break;

			usleep(100000);		/* 100ms */
			try_count --;
		} while(try_count);

		if (try_count >= 100000) {
			ptl_warn("Shared memory file has wrong size\n");
			goto exit_fail;
		}
	}

	/* Fill our portion of the comm pad. */
	ni->shmem.comm_pad =
		(uint8_t *)mmap(NULL, ni->shmem.comm_pad_size,
						PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (ni->shmem.comm_pad == MAP_FAILED) {
		ptl_warn("mmap failed (%d)", errno);
		perror("");
		goto exit_fail;
	}

	/* The share memory is mmaped, so we can close the file. */
	close(shm_fd);
	shm_fd = -1;

	/* Now we can create the buffer pool */
	ni->shmem.first_queue = ni->shmem.comm_pad + pid_table_size;
	ni->shmem.queue = (queue_t *)(ni->shmem.first_queue +
								  (ni->shmem.per_proc_comm_buf_size*ni->mem.index));
	queue_init(ni->shmem.queue);

	/* The buffer is right after the nemesis queue. */
	ni->sbuf_pool.pre_alloc_buffer = (void *)(ni->shmem.queue + 1);

	err = pool_init(ni->iface->gbl, &ni->sbuf_pool, "sbuf", real_buf_t_size(),
					POOL_SBUF, (obj_t *)ni);
	if (err) {
		WARN();
		goto exit_fail;
	}

#if !USE_KNEM
	/* Initialize the bounce buffers and let index 0 link them
	 * together. */
	ni->shmem.bounce_buf.head = ni->shmem.comm_pad + bounce_head_offset;
	ni->shmem.bounce_buf.bbs = ni->shmem.comm_pad + bounce_buf_offset;

	if (ni->mem.index == 0) {
		ni->shmem.bounce_buf.head->head_index0 = ni->shmem.bounce_buf.head;
		ll_init(&ni->shmem.bounce_buf.head->free_list);

		for (i = 0; i < ni->shmem.bounce_buf.num_bufs; i++) {
			void *bb = ni->shmem.bounce_buf.bbs + i*ni->shmem.bounce_buf.buf_size;

			ll_enqueue_obj(&ni->shmem.bounce_buf.head->free_list, bb);
		}
	}
#endif

	if (ni->options & PTL_NI_LOGICAL) {
		/* Can now announce my presence. */

		/* The PID table is a the beginning of the comm pad. */
		struct shmem_pid_table *pid_table =
			(struct shmem_pid_table *)ni->shmem.comm_pad;

		pid_table[ni->mem.index].id = ni->id;
		__sync_synchronize(); /* ensure "valid" is not written before pid. */
		pid_table[ni->mem.index].valid = 1;

		/* Now, wait for my siblings to get here. */
		for (i = 0; i < ni->mem.node_size; ++i) {
			/* oddly enough, this should reduce cache traffic
			 * for large numbers of siblings */
			while (pid_table[i].valid == 0)
				SPINLOCK_BODY();
		}

		/* All ranks have mmaped the memory. Get rid of the file. */
		shm_unlink(ni->shmem.comm_pad_shm_name);
		free(ni->shmem.comm_pad_shm_name);
		ni->shmem.comm_pad_shm_name = NULL;
	} else {
		/* Physical interface. We are connected to ourselves. */
		conn_t *conn = get_conn(ni, ni->id);

		if (!conn) {
			/* It's hard to recover from here. */
			WARN();
			goto exit_fail;
		}

		conn->transport = transport_shmem;
		conn->state = CONN_STATE_CONNECTED;

		conn_put(conn);			/* from get_conn */
	}

	return PTL_OK;

 exit_fail:
	if (shm_fd != -1)
		close(shm_fd);

	release_shmem_resources(ni);

	return PTL_FAIL;
}

/**
 * @brief Cleanup shared memory resources.
 *
 * @param[in] ni
 */
void cleanup_shmem(ni_t *ni)
{
	release_shmem_resources(ni);
}

/**
 * @brief enqueue a buf to a pid using shared memory.
 *
 * @param[in] ni the network interface
 * @param[in] buf the buf
 * @param[in] dest the destination pid
 */
void shmem_enqueue(ni_t *ni, buf_t *buf, ptl_pid_t dest)
{
	queue_t *queue = (queue_t *)(ni->shmem.first_queue +
			   (ni->shmem.per_proc_comm_buf_size * dest));

	buf->obj.next = NULL;

	enqueue(ni->shmem.comm_pad, queue, &buf->obj);
}

/**
 * @brief dequeue a buf using shared memory.
 *
 * @param[in] ni the network interface.
 */
buf_t *shmem_dequeue(ni_t *ni)
{
	return (buf_t *)dequeue(ni->shmem.comm_pad, ni->shmem.queue);
}

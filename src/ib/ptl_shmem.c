/*
 * ptl_shmem.c
 */

#include "ptl_loc.h"

static int send_message_shmem(buf_t *buf, int signaled)
{
	xi_t *xi = buf->xi;

	/* Keep a reference on the buffer so it doesn't get freed. will be
	 * returned by the remote side with type=BUF_SHMEM_RETURN. */ 
	buf_get(buf);

	buf->type = BUF_SHMEM;
	buf->comp = signaled;

	buf->shmem.source = buf->obj.obj_ni->shmem.index;

	assert(buf->xt->conn->shmem.local_rank == xi->dest.shmem.local_rank);

	assert(xi->ack_buf == NULL);
	if (!signaled) {
		xi->ack_buf = buf;
	}

	PtlInternalFragmentToss(buf->obj.obj_ni, buf, xi->dest.shmem.local_rank);

	return PTL_OK;
}

/*
 * build_rdma_sge
 *	Build the local scatter gather list for a target RDMA operation.
 *
 * Returns the number of bytes to be transferred by the SG list.
 */
static ptl_size_t do_knem_copy(xt_t *xt,
							   ptl_size_t rem_len, uint64_t rcookie, uint64_t roffset,
							   ptl_size_t *loc_index, ptl_size_t *loc_off,
							   int max_loc_index,
							   data_dir_t dir)
{
	me_t *me = xt->me;
	ni_t *ni = me->obj.obj_ni;
	ptl_iovec_t *iov;
	ptl_size_t tot_len = 0;
	ptl_size_t len;
	mr_t *mr;

	while (tot_len < rem_len) {
		len = rem_len - tot_len;
		if (me->num_iov) {
			void *addr;
			int err;

			if (*loc_index >= max_loc_index) {
				WARN();
				break;
			}
			iov = ((ptl_iovec_t *)me->start) + *loc_index;

			addr = iov->iov_base + *loc_off;
			if (len > iov->iov_len - *loc_off)
				len = iov->iov_len - *loc_off;

			err = mr_lookup(xt->obj.obj_ni, addr, len, &mr);
			if (err) {
				WARN();
				break;
			}

			if (dir == DATA_DIR_IN)
				err = knem_copy(ni, rcookie, roffset, 
								mr->knem_cookie, addr - mr->ibmr->addr,
								len);
			else
				err = knem_copy(ni, 
								mr->knem_cookie, addr - mr->ibmr->addr,
								rcookie, roffset,
								len);

			/* TODO: cache it ? */
			mr_put(mr);

			tot_len += len;
			*loc_off += len;
			roffset += len;
			if (tot_len < rem_len && *loc_off >= iov->iov_len) {
				if (*loc_index < max_loc_index) {
					*loc_off = 0;
					(*loc_index)++;
				} else
					break;
			}
		} else {
			int err;
			mr_t *mr;
			void *addr;
			ptl_size_t len_available = me->length - *loc_off;

			assert(me->length > *loc_off);
				
			if (len > len_available)
				len = len_available;

			addr = me->start + *loc_off;

			err = mr_lookup(ni, addr, len, &mr);
			if (err) {
				WARN();
				break;
			}

			if (dir == DATA_DIR_IN)
				knem_copy(ni, rcookie, roffset, 
						  mr->knem_cookie, addr - mr->ibmr->addr,
						  len);
			else
				knem_copy(ni, 
						  mr->knem_cookie, addr - mr->ibmr->addr,
						  rcookie, roffset,
						  len);

			/* TODO: cache it ? */
			mr_put(mr);

			tot_len += len;
			*loc_off += len;
			roffset += len;
			if (tot_len < rem_len && *loc_off >= mr->ibmr->length)
				break;
		}
	}

	return tot_len;
}

/*
 * do_knem_transfer
 *	Issue one or more RDMA from target to initiator based on target
 * transfer state.
 *
 * xt - target transfer context
 * dir - direction of transfer from target perspective
 */
static int do_knem_transfer(xt_t *xt)
{
	uint64_t rcookie;
	uint64_t roffset;
	ptl_size_t bytes;
	ptl_size_t iov_index = xt->cur_loc_iov_index;
	ptl_size_t iov_off = xt->cur_loc_iov_off;
	uint32_t rlength;
	uint32_t rseg_length;
	data_dir_t dir = xt->rdma_dir;
	ptl_size_t *resid = (dir == DATA_DIR_IN) ? &xt->put_resid : &xt->get_resid;

	rseg_length = xt->shmem.cur_rem_iovec->length;
	rcookie = xt->shmem.cur_rem_iovec->cookie;
	roffset = xt->shmem.cur_rem_iovec->offset;

	while (*resid > 0) {

		roffset += xt->shmem.cur_rem_off;
		rlength = rseg_length - xt->shmem.cur_rem_off;

		if (debug)
			printf("rcookie(0x%" PRIx64 "), rlen(%d)\n", rcookie, rlength);

		if (rlength > *resid)
			rlength = *resid;

		bytes = do_knem_copy(xt, rlength, rcookie, roffset,
							 &iov_index, &iov_off, xt->le->num_iov,
							 dir);
		if (!bytes) {
			WARN();
			return PTL_FAIL;
		}

		*resid -= bytes;
		xt->cur_loc_iov_index = iov_index;
		xt->cur_loc_iov_off = iov_off;
		xt->shmem.cur_rem_off += bytes;

		if (*resid && xt->shmem.cur_rem_off >= rseg_length) {
			if (xt->shmem.num_rem_iovecs) {
				xt->shmem.cur_rem_iovec++;
				rseg_length = xt->shmem.cur_rem_iovec->length;
				rcookie = xt->shmem.cur_rem_iovec->cookie;
				roffset = xt->shmem.cur_rem_iovec->offset;
				xt->shmem.cur_rem_off = 0;
			} else {
				WARN();
				return PTL_FAIL;
			}
		}
	}

	if (debug)
		printf("DMA done, resid(%d)\n", (int) *resid);

	return PTL_OK;
}

struct transport transport_shmem = {
	.type = CONN_TYPE_SHMEM,

	.post_tgt_dma = do_knem_transfer,
	.send_message = send_message_shmem,
};

/*
 * send_comp
 *	process a send completion event
 */
static void send_comp_shmem(buf_t *buf)
{
	if (buf->comp) {
		assert(buf->xt);
		buf_put(buf);
	}
}

static void *PtlInternalDMCatcher(void *param)
{
	ni_t *ni = param;
	int must_stop = 0;

	/* Even if we must stop, keep looping until all buffers are
	 * returned. */
    while (!must_stop || ni->sbuf_pool.count) {
        buf_t *shmem_buf;
		int err;

		shmem_buf = PtlInternalFragmentReceive(ni);

		switch(shmem_buf->type) {
		case BUF_SHMEM: {
			buf_t *buf;

			err = buf_alloc(ni, &buf);
			if (err) {
				WARN();
			} else {
				buf->data = (hdr_t *)shmem_buf->internal_data;
				buf->length = shmem_buf->length;
				INIT_LIST_HEAD(&buf->list);
				process_recv_shmem(ni, buf);

				/* Send the buffer back. */
				shmem_buf->type = BUF_SHMEM_RETURN;
				PtlInternalFragmentToss(ni, shmem_buf, shmem_buf->shmem.source);
			}
		}
			break;

		case BUF_SHMEM_RETURN:
			/* Buffer returned to us by remote node. */
			assert(shmem_buf->shmem.source == ni->shmem.index);

			send_comp_shmem(shmem_buf);

			/* From send_message_shmem(). */
			buf_put(shmem_buf);
			break;

		case BUF_SHMEM_STOP:
			buf_put(shmem_buf);
			/* Exit the process */
			must_stop = 1;
			break;

		default:
			/* Should not happen. */
			abort();
		}
	}

	return NULL;
}

static void PtlInternalDMTeardown(ni_t *ni)
{
	buf_t *buf;
	int err;

	err = sbuf_alloc(ni, &buf);
	if (err)
		return;

	buf->type = BUF_SHMEM_STOP;
	PtlInternalFragmentToss(ni, buf, ni->shmem.index);

	if (ni->shmem.has_catcher) {
		ptl_assert(pthread_join(ni->shmem.catcher, NULL), 0);
		ni->shmem.has_catcher = 0;
	}
}

int PtlNIInit_shmem(iface_t *iface, ni_t *ni)
{
	int shm_fd = -1;
	char comm_pad_shm_name[200] = "";
	int err;

	/* 
	 * Buffers in shared memory. The buffers will be allocated later,
	 * but not by the pool management. We compute the sizes now.
	 */
	/* Allocate a pool of buffers in the mmapped region.
	 * TODO: make 512 a parameter. */
	ni->shmem.per_proc_comm_buf_numbers = 512;

	ni->sbuf_pool.setup = buf_setup;
	ni->sbuf_pool.init = buf_init;
	ni->sbuf_pool.cleanup = buf_cleanup;
	ni->sbuf_pool.use_pre_alloc_buffer = 1;
	ni->sbuf_pool.round_size = real_buf_t_size();
	ni->sbuf_pool.slab_size = ni->shmem.per_proc_comm_buf_numbers * ni->sbuf_pool.round_size;

	/* Open KNEM device */
	if (knem_init(ni))
		goto exit_fail;

	/* Create a unique name for the shared memory file. Use the hash
	 * created from the mapping. */
	snprintf(comm_pad_shm_name, sizeof(comm_pad_shm_name), "/portals4-shmem-%x-%d", ni->shmem.hash, ni->options);
	ni->shmem.comm_pad_shm_name = strdup(comm_pad_shm_name);

	/* Allocate a pool of buffers in the mmapped region. */
	ni->shmem.per_proc_comm_buf_size =
		sizeof(NEMESIS_blocking_queue) +
		ni->sbuf_pool.slab_size;

	ni->shmem.comm_pad_size = pagesize +
		(ni->shmem.per_proc_comm_buf_size * (ni->shmem.world_size + 1));                  // the one extra is for the collator

	/* Open the communication pad. Let rank 0 create the shared memory. */
	assert(ni->shmem.comm_pad == NULL);

	if (ni->shmem.index == 0) {
		/* Just in case, remove that file if it already exist. */
		shm_unlink(comm_pad_shm_name);

		shm_fd = shm_open(comm_pad_shm_name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		assert(shm_fd >= 0);

		if (shm_fd < 0) {
			ptl_warn("shm_open failed (errno=%d)", errno);
			goto exit_fail;
		}

		/* Enlarge the memory zone to the size we need. */
		if (ftruncate(shm_fd, ni->shmem.comm_pad_size) != 0) {
			ptl_warn("share memory ftruncate failed");
			shm_unlink(comm_pad_shm_name);
			goto exit_fail;
		}
	} else {
		/* Try for 10 seconds. That should leave enough time for rank
		 * 0 to create the file. */
		int try_count = 100;

		do {
			shm_fd = shm_open(comm_pad_shm_name, O_RDWR, S_IRUSR | S_IWUSR);

			if (shm_fd != -1)
				break;

			usleep(100000);		/* 100ms */
			try_count --;
		} while(try_count);

		if (shm_fd == -1) {
			ptl_warn("Couln't open the shared memory file\n");
			goto exit_fail;
		}
	}

	/* Fill our portion of the comm pad. */
	ni->shmem.comm_pad =
		(uint8_t *)mmap(NULL, ni->shmem.comm_pad_size, PROT_READ | PROT_WRITE,
						MAP_SHARED, shm_fd, 0);
	if (ni->shmem.comm_pad == MAP_FAILED) {
		ptl_warn("mmap failed (%d)", errno);
		perror("");
		goto exit_fail;
	}

	/* The share memory is mmaped, so we can close the file. */
	close(shm_fd);
	shm_fd = -1;

	/* Now we can create the buffer pool */
	PtlInternalFragmentSetup(ni);

	/* The buffer is right after the nemesis queue. */
	ni->sbuf_pool.pre_alloc_buffer = (void *)(ni->shmem.receiveQ + 1);

	err = pool_init(&ni->sbuf_pool, "sbuf", real_buf_t_size(),
					POOL_SBUF, (obj_t *)ni);
	if (err) {
		WARN();
		goto exit_fail;
	}

	return PTL_OK;

 exit_fail:
	if (shm_fd != -1)
		close(shm_fd);

	if (comm_pad_shm_name[0] != 0)
		shm_unlink(comm_pad_shm_name);

    return PTL_FAIL;
}

/* Finish the initialization. This can only be done once we know the
 * mapping. */
int PtlNIInit_shmem_part2(ni_t *ni)
{
	int i;
	struct shmem_pid_table *ptable;
	int ret;

	/* Can Now Announce My Presence. */

	/* The PID table is a the beginning of the comm pad. 
	 *
	 * TODO: there's an assumption that a page size is enough
	 * (ie. that is enough for 341 local ranks).. */
	ptable = (struct shmem_pid_table *)ni->shmem.comm_pad;

	ptable[ni->shmem.index].id = ni->id;
	__sync_synchronize(); /* ensure "valid" is not written before pid. */
	ptable[ni->shmem.index].valid = 1;

	/* Now, wait for my siblings to get here. */
	for (i = 0; i < ni->shmem.world_size; ++i) {
		conn_t *conn;

		/* oddly enough, this should reduce cache traffic for large numbers
		 * of siblings */
		while (ptable[i].valid == 0)
			SPINLOCK_BODY();

		/* Reconfigure this connection to go through SHMEM instead of
		 * the default. */
		conn = get_conn(ni, &ptable[i].id);
		if (!conn) {
			/* It's hard to recover from here. */
			ret = PTL_FAIL;
			goto exit_fail;
		}

		conn->transport = transport_shmem;
		conn->state = CONN_STATE_CONNECTED;
		conn->shmem.local_rank = i;
	}

	/* Create the thread that will wait on the receive queue. */
	ptl_assert(pthread_create(&ni->shmem.catcher, NULL, PtlInternalDMCatcher, ni), 0);
	ni->shmem.has_catcher = 1;

	ret = PTL_OK;

 exit_fail:
	/* Destroy the mmaped file so it doesn't pollute. All ranks try it
	 * in case rank 0 died. */
	shm_unlink(ni->shmem.comm_pad_shm_name);
	free(ni->shmem.comm_pad_shm_name);
	ni->shmem.comm_pad_shm_name = NULL;

	return ret;
}

void cleanup_shmem(ni_t *ni)
{
	PtlInternalDMTeardown(ni);

	pool_fini(&ni->sbuf_pool);

	if (ni->shmem.comm_pad) {
		munmap(ni->shmem.comm_pad, ni->shmem.comm_pad_size);
		ni->shmem.comm_pad = NULL;
		ni->shmem.comm_pad_size = 0;
	}

	knem_fini(ni);
}

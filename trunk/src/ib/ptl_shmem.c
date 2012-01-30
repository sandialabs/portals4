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
	buf_get(buf);

	buf->type = BUF_SHMEM_SEND;

	buf->shmem.index_owner = buf->obj.obj_ni->shmem.index;

	shmem_enqueue(buf->obj.obj_ni, buf,
				buf->dest.shmem.local_rank);

	return PTL_OK;
}

/**
 * @brief Do a shared memory copy using the knem device.
 *
 * @param[in] buf
 * @param[in] rem_len
 * @param[in] rcookie
 * @param[in] roffset
 * @param[in,out] loc_index
 * @param[in,out] loc_off
 * @param[in] max_loc_index
 * @param[in] dir
 *
 * @return the number of bytes to be transferred by the SG list.
 */
static ptl_size_t do_knem_copy(buf_t *buf, ptl_size_t rem_len,
			       uint64_t rcookie, uint64_t roffset,
			       ptl_size_t *loc_index, ptl_size_t *loc_off,
			       int max_loc_index, data_dir_t dir)
{
	ni_t *ni = obj_to_ni(buf);
	me_t *me = buf->me;
	ptl_iovec_t *iov;
	ptl_size_t tot_len = 0;
	ptl_size_t len;
	mr_t *mr;

	while (tot_len < rem_len) {
		len = rem_len - tot_len;
		if (me->num_iov) {
			void *addr;
			int err;

			if (*loc_index >= max_loc_index)
				break;

			iov = ((ptl_iovec_t *)me->start) + *loc_index;

			addr = iov->iov_base + *loc_off;

			if (len > iov->iov_len - *loc_off)
				len = iov->iov_len - *loc_off;

			err = mr_lookup(buf->obj.obj_ni, addr, len, &mr);
			if (err)
				break;

			if (dir == DATA_DIR_IN)
				err = knem_copy(ni, rcookie, roffset, 
						mr->knem_cookie,
						addr - mr->addr, len);
			else
				err = knem_copy(ni, mr->knem_cookie,
						addr - mr->addr,
						rcookie, roffset, len);

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
			if (err)
				break;

			if (dir == DATA_DIR_IN)
				knem_copy(ni, rcookie, roffset, 
					  mr->knem_cookie,
					  addr - mr->addr, len);
			else
				knem_copy(ni, mr->knem_cookie,
					  addr - mr->addr, rcookie,
					  roffset, len);

			mr_put(mr);

			tot_len += len;
			*loc_off += len;
			roffset += len;
			if (tot_len < rem_len && *loc_off >= mr->length)
				break;
		}
	}

	return tot_len;
}

/**
 * @brief Complete a data phase using shared memory knem device.
 *
 * @param[in] buf
 *
 * @return status
 */
static int do_knem_transfer(buf_t *buf)
{
	uint64_t rcookie;
	uint64_t roffset;
	ptl_size_t bytes;
	ptl_size_t iov_index = buf->cur_loc_iov_index;
	ptl_size_t iov_off = buf->cur_loc_iov_off;
	uint32_t rlength;
	uint32_t rseg_length;
	data_dir_t dir = buf->rdma_dir;
	ptl_size_t *resid = (dir == DATA_DIR_IN) ?
				&buf->put_resid : &buf->get_resid;

	rseg_length = buf->shmem.cur_rem_iovec->length;
	rcookie = buf->shmem.cur_rem_iovec->cookie;
	roffset = buf->shmem.cur_rem_iovec->offset;

	while (*resid > 0) {

		roffset += buf->shmem.cur_rem_off;
		rlength = rseg_length - buf->shmem.cur_rem_off;

		if (rlength > *resid)
			rlength = *resid;

		bytes = do_knem_copy(buf, rlength, rcookie, roffset,
				     &iov_index, &iov_off, buf->le->num_iov,
							 dir);
		if (!bytes)
			return PTL_FAIL;

		*resid -= bytes;
		buf->cur_loc_iov_index = iov_index;
		buf->cur_loc_iov_off = iov_off;
		buf->shmem.cur_rem_off += bytes;

		if (*resid && buf->shmem.cur_rem_off >= rseg_length) {
			if (buf->shmem.num_rem_iovecs) {
				buf->shmem.cur_rem_iovec++;
				rseg_length = buf->shmem.cur_rem_iovec->length;
				rcookie = buf->shmem.cur_rem_iovec->cookie;
				roffset = buf->shmem.cur_rem_iovec->offset;
				buf->shmem.cur_rem_off = 0;
			} else {
				return PTL_FAIL;
			}
		}
	}

	return PTL_OK;
}

struct transport transport_shmem = {
	.type = CONN_TYPE_SHMEM,
	.post_tgt_dma = do_knem_transfer,
	.send_message = send_message_shmem,
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
	int shm_fd = -1;
	char comm_pad_shm_name[200] = "";
	int err;
	int i;
	struct shmem_pid_table *ptable;

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
	if (knem_init(ni))
		goto exit_fail;

	/* Create a unique name for the shared memory file. Use the hash
	 * created from the mapping. */
	snprintf(comm_pad_shm_name, sizeof(comm_pad_shm_name),
		 "/portals4-shmem-%x-%d", ni->shmem.hash, ni->options);
	ni->shmem.comm_pad_shm_name = strdup(comm_pad_shm_name);

	/* Allocate a pool of buffers in the mmapped region. */
	ni->shmem.per_proc_comm_buf_size =
		sizeof(queue_t) +
		ni->sbuf_pool.slab_size;

	ni->shmem.comm_pad_size = pagesize +
		(ni->shmem.per_proc_comm_buf_size * ni->shmem.world_size);

	/* Open the communication pad. Let rank 0 create the shared memory. */
	assert(ni->shmem.comm_pad == MAP_FAILED);

	if (ni->shmem.index == 0) {
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
			ptl_warn("Couln't open the shared memory file\n");
			goto exit_fail;
		}

		/* Wait for the file to have the right size before mmaping
		 * it. */
		try_count = 100;
		do {
			struct stat buf;

			if (fstat(shm_fd, &buf) == -1) {
				ptl_warn("Couln't fstat the shared memory file\n");
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
	queue_init(ni);

	/* The buffer is right after the nemesis queue. */
	ni->sbuf_pool.pre_alloc_buffer = (void *)(ni->shmem.queue + 1);

	err = pool_init(&ni->sbuf_pool, "sbuf", real_buf_t_size(),
					POOL_SBUF, (obj_t *)ni);
	if (err) {
		WARN();
		goto exit_fail;
	}

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

		/* oddly enough, this should reduce cache traffic
		 * for large numbers of siblings */
		while (ptable[i].valid == 0)
			SPINLOCK_BODY();

		/* Reconfigure this connection to go through SHMEM
		 * instead of the default. */
		conn = get_conn(ni, ptable[i].id);
		if (!conn) {
			/* It's hard to recover from here. */
			goto exit_fail;
		}

		conn->transport = transport_shmem;
		conn->state = CONN_STATE_CONNECTED;
		conn->shmem.local_rank = i;

		conn_put(conn);			/* from get_conn */
	}

	/* All ranks have mmaped the memory. Get rid of the file. */
	shm_unlink(ni->shmem.comm_pad_shm_name);
	free(ni->shmem.comm_pad_shm_name);
	ni->shmem.comm_pad_shm_name = NULL;

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

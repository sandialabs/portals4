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

struct transport transport_shmem = {
	.type = CONN_TYPE_SHMEM,
	.buf_alloc = sbuf_alloc,
	.post_tgt_dma = do_mem_transfer,
	.send_message = send_message_shmem,
	.set_send_flags = shmem_set_send_flags,
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

	ni->shmem.comm_pad_size = pagesize +
		(ni->shmem.per_proc_comm_buf_size * ni->mem.node_size);

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
	ni->shmem.queue = (queue_t *)(ni->shmem.comm_pad + pagesize +
						(ni->shmem.per_proc_comm_buf_size*ni->mem.index));
	queue_init(ni->shmem.queue);

	/* The buffer is right after the nemesis queue. */
	ni->sbuf_pool.pre_alloc_buffer = (void *)(ni->shmem.queue + 1);

	err = pool_init(&ni->sbuf_pool, "sbuf", real_buf_t_size(),
					POOL_SBUF, (obj_t *)ni);
	if (err) {
		WARN();
		goto exit_fail;
	}

	if (ni->options & PTL_NI_LOGICAL) {
		/* Can now announce my Presence. */

		/* The PID table is a the beginning of the comm pad. 
		 *
		 * TODO: there's an assumption that a page size is enough
		 * (ie. that is enough for 341 local ranks).. */
		ptable = (struct shmem_pid_table *)ni->shmem.comm_pad;

		ptable[ni->mem.index].id = ni->id;
		__sync_synchronize(); /* ensure "valid" is not written before pid. */
		ptable[ni->mem.index].valid = 1;

		/* Now, wait for my siblings to get here. */
		for (i = 0; i < ni->mem.node_size; ++i) {
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
				WARN();
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
	queue_t *queue = (queue_t *)(ni->shmem.comm_pad + pagesize +
			   (ni->shmem.per_proc_comm_buf_size * dest));

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

/*
 * ptl_shmem.c
 */

#include "ptl_loc.h"

/*
 * send_comp
 *	process a send completion event
 */
static void send_comp_shmem(buf_t *buf)
{
	struct list_head temp_list;

	if (buf->comp) {
		xt_t *xt = buf->xt;

		assert(xt);

		/* On the send list. */
		pthread_spin_lock(&xt->send_list_lock);
		list_cut_position(&temp_list, &xt->send_list, &buf->list);
		pthread_spin_unlock(&xt->send_list_lock);

		while(!list_empty(&temp_list)) {
			buf = list_first_entry(&temp_list, buf_t, list);
			list_del(&buf->list);
			buf_put(buf);
		}
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
			assert(shmem_buf->shmem.source == ni->shmem.local_rank);

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
	PtlInternalFragmentToss(ni, buf, ni->shmem.local_rank);

	ptl_assert(pthread_join(ni->shmem.shmem_catcher, NULL), 0);
}

#define PARSE_ENV_NUM(env_str, var, reqd) do {							\
		char       *strerr;												\
		const char *str = getenv(env_str);								\
		if (str == NULL) {												\
			if (reqd == 1) { goto exit_fail; }							\
		} else {														\
			size_t tmp = strtol(str, &strerr, 10);						\
			if ((strerr == NULL) || (strerr == str) || (*strerr != 0)) { \
				goto exit_fail;											\
			}															\
			var = tmp;													\
		}																\
	} while (0)


int PtlNIInit_shmem(iface_t *iface, ni_t *ni)
{
	int shm_fd = -1;
	char comm_pad_shm_name[200] = "";
	struct shmem_pid_table *ptable;
	int i;
	char *env;

	/* Open KNEM device */
	if (knem_init(ni))
		goto exit_fail;

	/* Get the environemnt Job ID to create a unique name for the shared memory file. */
	env = getenv("OMPI_MCA_orte_ess_jobid");
	if (!env) {
		ptl_warn("Environment variable OMPI_MCA_orte_ess_jobid missing\n"); 
		goto exit_fail;
	}
	snprintf(comm_pad_shm_name, sizeof(comm_pad_shm_name), "/portals4-shmem-%s", env);

	/* Allocate a pool of buffers in the mmapped region. */
	ni->shmem.per_proc_comm_buf_size =
		sizeof(NEMESIS_blocking_queue) +
		ni->sbuf_pool.segment_size;

	ni->shmem.comm_pad_size = pagesize +
		(ni->shmem.per_proc_comm_buf_size * (ni->shmem.num_siblings + 1));                  // the one extra is for the collator

	/* Open the communication pad. Let rank 0 create the shared memory. */
	assert(ni->shmem.comm_pad == NULL);
	if (ni->shmem.local_rank == 0) {
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

	PtlInternalFragmentSetup(ni);

	/* Can Now Announce My Presence. */

	/* The PID table is a the beginning of the comm pad. 
	 *
	 * TODO: there's an assumption that a page size is enough
	 * (ie. that is enough for 341 local ranks).. */
	ptable = (struct shmem_pid_table *)ni->shmem.comm_pad;

	ptable[ni->shmem.local_rank].id = ni->id;
	__sync_synchronize(); /* ensure "valid" is not written before pid. */
	ptable[ni->shmem.local_rank].valid = 1;

	/* Now, wait for my siblings to get here. */
	for (i = 0; i < ni->shmem.num_siblings; ++i) {
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
			goto exit_fail;
		}

		conn->transport = transport_shmem;
		conn->state = CONN_STATE_CONNECTED;
		conn->shmem.local_rank = i;
	}

	/* Create the thread that will wait on the receive queue. */
	ptl_assert(pthread_create(&ni->shmem.shmem_catcher, NULL, PtlInternalDMCatcher, ni), 0);

	/* Destroy the mmaped file so it doesn't pollute. All ranks try it
	 * in case rank 0 died. */
	shm_unlink(comm_pad_shm_name);

	return PTL_OK;

 exit_fail:
	if (shm_fd != -1)
		close(shm_fd);

	if (comm_pad_shm_name[0] != 0)
		shm_unlink(comm_pad_shm_name);

    return PTL_FAIL;
}

void cleanup_shmem(ni_t *ni)
{
	PtlInternalDMTeardown(ni);

	if (ni->shmem.comm_pad) {
		munmap(ni->shmem.comm_pad, ni->shmem.comm_pad_size);
		ni->shmem.comm_pad = NULL;
		ni->shmem.comm_pad_size = 0;
	}

	knem_fini(ni);
}

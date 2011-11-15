/**
 * @file ptl_buf.h
 *
 * Buf object methods.
 */

#include "ptl_loc.h"

/**
 * Setup a buf.
 *
 * Called each time the buf is allocated from the buf pool freelist.
 *
 * @param arg opaque reference to buf
 *
 * @return status
 */
int buf_setup(void *arg)
{
	buf_t *buf = arg;

	buf->num_mr = 0;
	buf->comp = 0;
	buf->data = buf->internal_data;
	buf->rdma.recv.wr.next = NULL;
	buf->rdma_desc_ok = 0;
	buf->ni_fail = PTL_NI_OK;

	return PTL_OK;
}

/**
 * Cleanup a buf.
 *
 * Called each time buf is freed to the buf pool.
 *
 * @param arg opaque reference to buf
 */ 
void buf_cleanup(void *arg)
{
	buf_t *buf = arg;
	int i;

	for (i = 0; i < buf->num_mr; i++)
		mr_put(buf->mr_list[i]);

	buf->num_mr = 0;

	/* send/rdma bufs drop their references to
	 * the master buf here */
	if (buf->xxbuf) {
		buf_put(buf->xxbuf);
		buf->xxbuf = NULL;
	}

	buf->type = BUF_FREE;
}

/**
 * Init a buf.
 *
 * Called once when buf is created.
 * Sets information that is constant for the life
 * of the buf.
 *
 * @param buf which to init
 * @param parm parameter passed that contains the mr
 *
 * @return status
 */
int buf_init(void *arg, void *parm)
{
	buf_t *buf = arg;

	INIT_LIST_HEAD(&buf->list);

	buf->length = 0;
	buf->type = BUF_FREE;

	if (parm) {
		/* This buffer carries an MR, so it's an IB buffer, not a
		 * buffer in shared memory. */
		struct ibv_mr *mr = parm;

		buf->rdma.recv.wr.next = NULL;
		buf->rdma.recv.wr.wr_id = (uintptr_t)buf;
		buf->rdma.recv.wr.sg_list = &buf->rdma.recv.sg_list;
		buf->rdma.recv.wr.num_sge = 1;

		buf->rdma.recv.sg_list.addr = (uintptr_t)buf->internal_data;
		buf->rdma.recv.sg_list.lkey = mr->lkey;

		buf->rdma.lkey = mr->lkey;
	}

	pthread_spin_init(&buf->rdma_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_mutex_init(&buf->mutex, NULL);

	return 0;
}

void buf_fini(void *arg)
{
	buf_t *buf = arg;

	pthread_spin_destroy(&buf->rdma_list_lock);
}

/**
 * Debug print buf parameters.
 *
 * @param buf which to dump
 */
void buf_dump(buf_t *buf)
{
	hdr_t *hdr = (hdr_t *)buf->data;

	printf("buf: %p\n", buf);
	printf("buf->size	= %d\n", BUF_DATA_SIZE);
	printf("buf->length	= %d\n", buf->length);
	printf("hdr->version	= %d\n", hdr->version);
	printf("hdr->operation	= %d\n", hdr->operation);
	printf("hdr->ni_type	= %d\n", hdr->ni_type);
	printf("hdr->pkt_fmt	= %d\n", hdr->pkt_fmt);
	printf("hdr->length	= %" PRId64 "\n", le64_to_cpu(hdr->length));
	printf("\n");
}

/**
 * Post a receive buffer.
 *
 * Used to post receive buffers to the OFA verbs
 * shared receive queue (SRQ). A buf is allocated
 * from the ni's normal buf pool which takes a reference.
 * The buf is initialized as a receive buffer and is
 * posted to the SRQ. The buf is added to the ni
 * recv_list in the order that it was posted.
 *
 * @param ni for which to post receive buffer
 * @param count the desired number of buffers to post
 *
 * @return status
 */
int ptl_post_recv(ni_t *ni, int count)
{
	int err;
	buf_t *buf;
	struct ibv_recv_wr *bad_wr;
	int actual;
	struct ibv_recv_wr *wr;
	struct list_head list;

	if (count == 0)
		return PTL_OK;

	INIT_LIST_HEAD(&list);
	wr = NULL;

	for (actual = 0; actual < count; actual++) {
		err = buf_alloc(ni, &buf);
		if (err)
			break;

		buf->rdma.recv.sg_list.length = BUF_DATA_SIZE;
		buf->type = BUF_RECV;
		buf->rdma.recv.wr.next = wr;
		wr = &buf->rdma.recv.wr;

		list_add_tail(&buf->list, &list);
	}

	/* couldn't alloc any buffers */
	if (!actual) {
		WARN();
		return PTL_FAIL;
	}

	/* add buffers to ni recv_list for recovery during shutdown */
	pthread_spin_lock(&ni->rdma.recv_list_lock);
	list_splice_tail(&list, &ni->rdma.recv_list);
	pthread_spin_unlock(&ni->rdma.recv_list_lock);

	/* account for posted buffers */
	atomic_add(&ni->rdma.num_posted_recv, actual);

	err = ibv_post_srq_recv(ni->rdma.srq, &buf->rdma.recv.wr, &bad_wr);
	if (err) {
		WARN();

		/* re-stock any unposted buffers */
		pthread_spin_lock(&ni->rdma.recv_list_lock);
		for (wr = bad_wr; wr; wr = wr->next) {
			buf = container_of(wr, buf_t, rdma.recv.wr);
			list_del(&buf->list);
			buf_put(buf);

			/* account for failed buffers */
			atomic_dec(&ni->rdma.num_posted_recv);
		}
		pthread_spin_unlock(&ni->rdma.recv_list_lock);
	}

	return PTL_OK;
}

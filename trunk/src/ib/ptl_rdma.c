/**
 * @file ptl_rdma.c
 *
 * @brief RDMA operations used by target.
 */
#include "ptl_loc.h"

/**
 * @brief Build and post an RDMA read/write work request to transfer
 * data to/from one or more local memory segments from/to a single remote
 * memory segment.
 *
 * @param[in] buf The message buffer received by the target.
 * @param[in] rdma_buf A temporary buffer allocated by the
 * target to hold information about an rdma operation from the
 * initiator. There is (currently) one or more rdma_buf allocated for
 * each contiguous memory segment at the initiator but it may be
 * used to transfer data to multiple segments at the target by
 * using a scatter/gather list.
 * @param[in] raddr The remote address at the initiator.
 * @param[in] rkey The rkey of the InfiniBand MR that registers the
 * memory region at the initiator that includes the data segment.
 * @param[in] loc_sge The local scatter/gather array that contains
 * the local addresses, lengths and lkeys.
 * @param[in] num_loc_sge The size of the scatter/gather array.
 * @param[in] comp A flag indicating whether to generate a completion
 * event when this operation is complete.
 *
 * @return status
 */
int post_rdma(buf_t *buf, buf_t *rdma_buf, data_dir_t dir, uint64_t raddr,
	      uint32_t rkey, struct ibv_sge *loc_sge,
	      int num_loc_sge, uint8_t comp)
{
	int err;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;

	/* indicate whether a send completion event is expected */
	rdma_buf->comp = comp;

	/* build an infiniband rdma write work request */
	if (likely(comp)) {
		wr.wr_id = (uintptr_t) rdma_buf;
		wr.send_flags = IBV_SEND_SIGNALED;
	} else {
		wr.wr_id = 0;
		wr.send_flags = 0;
	}

	wr.next	= NULL;
	wr.sg_list = loc_sge;
	wr.num_sge = num_loc_sge;
	wr.opcode = (dir == DATA_DIR_IN) ? IBV_WR_RDMA_READ : IBV_WR_RDMA_WRITE;
	wr.wr.rdma.remote_addr = raddr;
	wr.wr.rdma.rkey	= rkey;
#ifdef USE_XRC
	wr.xrc_remote_srq_num = rdma_buf->dest.xrc_remote_srq_num;
#endif

	/* add the rdma_buf to a list of pending rdma
	 * transfers at the buf */
	pthread_spin_lock(&buf->rdma_list_lock);
	list_add_tail(&rdma_buf->list, &buf->rdma_list);
	pthread_spin_unlock(&buf->rdma_list_lock);

	/* post the work request to the QP send queue for the
	 * destination/initiator */
	err = ibv_post_send(buf->dest.rdma.qp, &wr, &bad_wr);
	if (err) {
		/* if the operation fails, dequeue the rdma_buf */
		if (comp) {
			pthread_spin_lock(&buf->rdma_list_lock);
			list_del(&rdma_buf->list);
			pthread_spin_unlock(&buf->rdma_list_lock);
		}

		return PTL_FAIL;
	}

	return PTL_OK;
}

/**
 * @brief Build the local scatter gather list for a target RDMA operation.
 *
 * The most general case is transfering from an iovec to an iovec.
 * This requires a double loop iterating over the memory segments
 * at the (remote) initiator and also over the memory segments in the
 * (local) target list element. This routine implements the loop over
 * the local memory segments building an InfiniBand scatter/gather
 * array to be used in an rdma operation. It is called by process_rdma
 * below which implements the outer loop over the remote memory segments.
 * The case where one or both the MD and the LE/ME do not have an iovec
 * are handled as limits of the general case.
 *
 * @param[in] buf The message buffer received by the target.
 * @param[in] rdma_buf The temp buf for next rdma operation.
 * @param[in,out] cur_index_p The current index in the LE/ME iovec.
 * @param[in,out] cur_off_p The offset into the current LE/ME
 * iovec element.
 * @param[in] sge The scatter/gather array to fill in.
 * @param[in] sge_size The size of the scatter/gather array.
 * @param[out] num_sge_p The number of sge entries used.
 * @param[in,out] length_p On input the requested number of bytes to be
 * transfered in the current rdma operation. On exit the actual number
 * of bytes transfered.
 *
 * @return status
 */
static int build_sge(buf_t *buf,
		     buf_t *rdma_buf,
		     ptl_size_t *cur_index_p,
		     ptl_size_t *cur_off_p,
		     struct ibv_sge *sge,
		     int sge_size,
		     int *num_sge_p,
		     ptl_size_t *length_p)
{
	int err;
	ni_t *ni = obj_to_ni(buf);
	me_t *me = buf->me;
	mr_t *mr;
	ptl_iovec_t *iov;
	ptl_size_t bytes;
	ptl_size_t cur_index = *cur_index_p;
	ptl_size_t cur_off = *cur_off_p;
	ptl_size_t cur_len = 0;
	int num_sge = 0;
	void * addr;
	ptl_size_t resid = *length_p;

	while (resid) {
		/* compute the starting address and
		 * length of the next sge entry */
		bytes = resid;

		if (unlikely(me->num_iov)) {
			iov = ((ptl_iovec_t *)me->start) + cur_index;
			addr = iov->iov_base + cur_off;

			if (bytes > iov->iov_len - cur_off)
				bytes = iov->iov_len - cur_off;
		} else {
			addr = me->start + cur_off;
			assert(bytes <= me->length - cur_off);
		}

		/* lookup the mr for the current local segment */
		err = mr_lookup(ni, addr, bytes, &mr);
		if (err)
			return err;

		sge->addr = (uintptr_t)addr;
		sge->length = bytes;
		sge->lkey = mr->ibmr->lkey;

		/* save the mr and the reference to it until
		 * we receive a completion */
		rdma_buf->mr_list[rdma_buf->num_mr++] = mr;

		/* update the dma info */
		resid -= bytes;
		cur_len += bytes;
		cur_off += bytes;

		if (unlikely(me->num_iov)) {
			if (cur_off >= iov->iov_len) {
				cur_index++;
				cur_off = 0;
			}
		}

		if (bytes) {
			sge++;
			if (++num_sge >= sge_size)
				break;
		}
	}

	*num_sge_p = num_sge;
	*cur_index_p = cur_index;
	*cur_off_p = cur_off;
	*length_p = cur_len;

	return PTL_OK;
}

/**
 * @brief Issue one or more InfiniBand RDMA from target to initiator
 * based on target transfer state.
 *
 * This routine is called from the tgt state machine for InfiniBand
 * transfers if there is data to transfer between initiator and
 * target that cannot be sent as immediate data.
 *
 * Each time this routine is called it issues as many rdma operations as
 * possible up to a limit or finishes the operation. The
 * current state of the rdma transfer(s) is contained in the buf->rdma
 * struct. Each rdma operation transfers data between one or more local
 * memory segments in an LE/ME and a single contiguous remote segment.
 * The number of local segments is limited by the size of the remote
 * segment and the maximum number of scatter/gather array elements.
 *
 * @param[in] buf The message buffer received by the target.
 *
 * @return status
 */
static int process_rdma(buf_t *buf)
{
	int err;
	uint64_t addr;
	ptl_size_t bytes;
	ptl_size_t iov_index;
	ptl_size_t iov_off;
	data_dir_t dir;
	ptl_size_t resid;
	int comp = 0;
	buf_t *rdma_buf;
	int sge_size = get_param(PTL_MAX_QP_SEND_SGE);
	struct ibv_sge sge[sge_size];
	int entries = 0;
	int cur_rdma_ops = 0;
	size_t rem_off;
	uint32_t rem_size;
	struct ibv_sge *rem_sge;
	uint32_t rem_key;
	int max_rdma_ops = get_param(PTL_MAX_RDMA_WR_OUT);

	dir = buf->rdma_dir;
	resid = (dir == DATA_DIR_IN) ? buf->put_resid : buf->get_resid;
	iov_index = buf->cur_loc_iov_index;
	iov_off = buf->cur_loc_iov_off;

	rem_sge = buf->rdma.cur_rem_sge;
	rem_off = buf->rdma.cur_rem_off;
	rem_size = le32_to_cpu(rem_sge->length);
	rem_key = le32_to_cpu(rem_sge->lkey);

	/* try to generate additional rdma operations as long
	 * as there is remaining data to transfer and we have
	 * not exceeded the maximum number of outstanding rdma
	 * operations that we allow ourselves. rdma_comp is
	 * incremented when we have reached this limit and
	 * will get cleared when we receive get send completions
	 * from the CQ. We do not reenter the state machine
	 * until we have received a send completion so
	 * rdma_comp should have been cleared */

	assert(!atomic_read(&buf->rdma.rdma_comp));

	while (resid) {
		/* compute remote starting address and
		 * and length of the next rdma transfer */
		addr = le64_to_cpu(rem_sge->addr) + rem_off;

		bytes = resid;
		if (bytes > rem_size - rem_off)
			bytes = rem_size - rem_off;

		rdma_buf = tgt_alloc_rdma_buf(buf);
		if (!rdma_buf)
			return PTL_FAIL;

		/* build a local scatter/gather array on our stack
		 * to transfer as many bytes as possible from the
		 * LE/ME up to rlength. The transfer size may be
		 * limited by the size of the scatter/gather list
		 * sge. The new values of iov_index and iov_offset
		 * are returned as well as the number of bytes
		 * transferred. */
		err = build_sge(buf, rdma_buf, &iov_index, &iov_off,
				sge, sge_size, &entries, &bytes);
		if (err) {
			buf_put(rdma_buf);
			return err;
		}

		/* update dma info */
		resid -= bytes;
		rem_off += bytes;

		if (resid && rem_off >= rem_size) {
			rem_sge++;
			rem_size = le32_to_cpu(rem_sge->length);
			rem_key = le32_to_cpu(rem_sge->lkey);
			rem_off = 0;
		}

		/* if we are finished or have reached the limit
		 * of the number of rdma's outstanding then
		 * request a completion notification */
		if (!resid || ++cur_rdma_ops >= max_rdma_ops) {
			comp = 1;
 			atomic_inc(&buf->rdma.rdma_comp);
 		}

		/* post the rdma read or write operation to the QP */
		err = post_rdma(buf, rdma_buf, dir, addr, rem_key, sge,
				entries, comp);
		if (err) {
			buf_put(rdma_buf);
			return err;
		}

		if (comp)
			break;
	}

	/* update the current rdma state */
	buf->cur_loc_iov_index = iov_index;
	buf->cur_loc_iov_off = iov_off;
	buf->rdma.cur_rem_off = rem_off;
	buf->rdma.cur_rem_sge = rem_sge;

	if (dir == DATA_DIR_IN)
		buf->put_resid = resid;
	else
		buf->get_resid = resid;

	return PTL_OK;
}

struct transport transport_rdma = {
	.type = CONN_TYPE_RDMA,
	.post_tgt_dma = process_rdma,
	.send_message = send_message_rdma,
};

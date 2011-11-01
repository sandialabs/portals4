/*
 * ptl_rdma.c - RDMA operations
 */
#include "ptl_loc.h"

/*
 * rdma read
 *	build and post a single RDMA read work request
 */
int rdma_read(buf_t *buf, buf_t *rdma_buf, uint64_t raddr, uint32_t rkey,
	struct ibv_sge *loc_sge, int num_loc_sge, uint8_t comp)
{
	int err;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;

	if (debug) {
		printf("send RDMA message, buf(%p)\n", rdma_buf);
		printf("radd(%" PRIx64 "), loc_sge->addr(%" PRIx64 "), "
			"num_sge(%d)\n", raddr, loc_sge[0].addr, num_loc_sge);
	}

	if (num_loc_sge > get_param(PTL_MAX_QP_SEND_SGE))
		return PTL_FAIL;

	rdma_buf->comp = comp;

	pthread_spin_lock(&buf->rdma_list_lock);
	list_add_tail(&rdma_buf->list, &buf->rdma_list);
	pthread_spin_unlock(&buf->rdma_list_lock);

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
	wr.opcode = IBV_WR_RDMA_READ;
	wr.wr.rdma.remote_addr = raddr;
	wr.wr.rdma.rkey	= rkey;

#ifdef USE_XRC
	wr.xrc_remote_srq_num = rdma_buf->dest.xrc_remote_srq_num;
#endif

	err = ibv_post_send(rdma_buf->dest.rdma.qp, &wr, &bad_wr);
	if (err) {
		WARN();
		if (comp) {
			pthread_spin_lock(&buf->rdma_list_lock);
			list_del(&rdma_buf->list);
			pthread_spin_unlock(&buf->rdma_list_lock);
		}

		return PTL_FAIL;
	}

	return PTL_OK;
}

/*
 * rdma_write
 *	build and post a single RDMA wrwite work request
 */
static int rdma_write(buf_t *buf, buf_t *rdma_buf, uint64_t raddr,
		      uint32_t rkey, struct ibv_sge *loc_sge,
		      int num_loc_sge, uint32_t imm_data, uint8_t imm,
		      uint8_t comp)
{
	int err;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;

	if (debug) {
		printf("post RDMA work, buf(%p)\n", rdma_buf);
		printf("radd(%" PRIx64 "), loc_sge->addr(%" PRIx64 "), "
			"num_sge(%d)\n", raddr, loc_sge[0].addr, num_loc_sge);
	}

	if (num_loc_sge > get_param(PTL_MAX_QP_SEND_SGE))
		return PTL_FAIL;

	rdma_buf->comp = comp;

	pthread_spin_lock(&buf->rdma_list_lock);
	list_add_tail(&rdma_buf->list, &buf->rdma_list);
	pthread_spin_unlock(&buf->rdma_list_lock);

	if (likely(comp)) {
		wr.wr_id = (uintptr_t) rdma_buf;
		wr.send_flags = IBV_SEND_SIGNALED;
	} else {
		wr.wr_id = 0;
		wr.send_flags = 0;
	}

	if (unlikely(imm)) {
		wr.imm_data = imm_data;
		wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
	} else
		wr.opcode = IBV_WR_RDMA_WRITE;

	wr.next	= NULL;
	wr.sg_list = loc_sge;
	wr.num_sge = num_loc_sge;
	wr.wr.rdma.remote_addr = raddr;
	wr.wr.rdma.rkey	= rkey;
#ifdef USE_XRC
	wr.xrc_remote_srq_num = rdma_buf->dest.xrc_remote_srq_num;
#endif

	err = ibv_post_send(rdma_buf->dest.rdma.qp, &wr, &bad_wr);
	if (err) {
		WARN();
		if (comp) {
			pthread_spin_lock(&buf->rdma_list_lock);
			list_del(&rdma_buf->list);
			pthread_spin_unlock(&buf->rdma_list_lock);
		}
		return PTL_FAIL;
	}

	return PTL_OK;
}

/*
 * build_rdma_sge
 *	Build the local scatter gather list for a target RDMA operation.
 *
 * Returns the number of bytes to be transferred by the SG list.
 */
static ptl_size_t build_rdma_sge(buf_t *buf, buf_t *rdma_buf,
				 ptl_size_t rem_len, struct ibv_sge *sge,
				 int num_sge, int *entries,
				 ptl_size_t *loc_index, ptl_size_t *loc_off,
				 int max_loc_index)
{
	me_t *me = buf->me;
	ptl_iovec_t *iov;
	ptl_size_t tot_len = 0;
	ptl_size_t len;
	int i = 0;
	ni_t *ni = obj_to_ni(buf);

	while (i < num_sge && tot_len < rem_len) {
		len = rem_len - tot_len;
		if (me->num_iov) {
			void * addr;
			int err;

			if (*loc_index >= max_loc_index) {
				WARN();
				break;
			}
			iov = ((ptl_iovec_t *)me->start) + *loc_index;

			addr = iov->iov_base + *loc_off;
			if (len > iov->iov_len - *loc_off)
				len = iov->iov_len - *loc_off;

			err = mr_lookup(ni, addr, len,
					&rdma_buf->mr_list[rdma_buf->num_mr]);
			if (err) {
				WARN();
				break;
			}

			sge->addr = (uintptr_t)addr;
			sge->length = len;
			sge->lkey = rdma_buf->mr_list[rdma_buf->num_mr]->ibmr->lkey;

			rdma_buf->num_mr++;

			tot_len += len;
			*loc_off += len;
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
			ptl_size_t len_available = me->length - *loc_off;

			assert(me->length > *loc_off);
				
			if (len > len_available)
				len = len_available;

			sge->addr = (uintptr_t)me->start + *loc_off;

			err = mr_lookup(me->obj.obj_ni, (void *)sge->addr, len, &mr);
			if (err) {
				WARN();
				break;
			}

			sge->lkey = mr->ibmr->lkey;
			sge->length = len;

			rdma_buf->mr_list[rdma_buf->num_mr] = mr;
			rdma_buf->num_mr ++;

			tot_len += len;
			*loc_off += len;
			if (tot_len < rem_len && *loc_off >= mr->ibmr->length)
				break;
		}

		/* Do not send 0 length segments. */
		if (len > 0) {
			i++;
			sge++;
		}
	}
	*entries = i;
	return tot_len;
}

/*
 * post_tgt_rdma
 *	Issue one or more RDMA from target to initiator based on target
 * transfer state.
 *
 * dir - direction of transfer from target perspective
 */
int post_tgt_rdma(buf_t *buf)
{
	uint64_t raddr;
	uint32_t rkey;
	ptl_size_t bytes;
	struct ibv_sge sge[get_param(PTL_MAX_QP_SEND_SGE)];
	int entries = 0;
	ptl_size_t iov_index = buf->cur_loc_iov_index;
	ptl_size_t iov_off = buf->cur_loc_iov_off;
	uint32_t rlength;
	uint32_t rseg_length;
	data_dir_t dir = buf->rdma_dir;
	ptl_size_t *resid = (dir == DATA_DIR_IN) ?
			&buf->put_resid : &buf->get_resid;
	int err;
	int comp = 0;
	buf_t *rdma_buf = NULL;

	rseg_length = le32_to_cpu(buf->rdma.cur_rem_sge->length);
	rkey  = le32_to_cpu(buf->rdma.cur_rem_sge->lkey);

	while (*resid > 0 && !atomic_read(&buf->rdma.rdma_comp)) {
		raddr = le64_to_cpu(buf->rdma.cur_rem_sge->addr) +
			buf->rdma.cur_rem_off;
		rlength = rseg_length - buf->rdma.cur_rem_off;

		if (debug)
			printf("raddr(0x%" PRIx64 "), rlen(%d), rkey(0x%x)\n",
			raddr, rlength, rkey);

		if (rlength > *resid)
			rlength = *resid;

		if (!rdma_buf) {
			rdma_buf = tgt_alloc_rdma_buf(buf);
			if (!rdma_buf) {
				WARN();
				return PTL_FAIL;
			}
		}

		bytes = build_rdma_sge(buf, rdma_buf, rlength, sge,
				       get_param(PTL_MAX_QP_SEND_SGE),
				       &entries, &iov_index,  &iov_off,
				       buf->le->num_iov);
		if (!bytes) {
			WARN();
			buf_put(rdma_buf);
			return PTL_FAIL;
		}

		buf->rdma.interim_rdma++;

		if (*resid == bytes ||
		    buf->rdma.interim_rdma >= get_param(PTL_MAX_RDMA_WR_OUT))
			comp = 1;

 		if (comp) {
 			atomic_inc(&buf->rdma.rdma_comp);
 			buf->rdma.interim_rdma = 0;
 		}

		if (dir == DATA_DIR_IN)
			err = rdma_read(buf, rdma_buf, raddr, rkey, sge,
				entries, comp);
		else
			err = rdma_write(buf, rdma_buf, raddr, rkey, sge,
				entries, 0, 0, comp);

		if (err) {
			WARN();
			buf_put(rdma_buf);
			return err;
		}

		rdma_buf = NULL;

		*resid -= bytes;
		buf->cur_loc_iov_index = iov_index;
		buf->cur_loc_iov_off = iov_off;
		buf->rdma.cur_rem_off += bytes;

		if (*resid && buf->rdma.cur_rem_off >= rseg_length) {
			if (buf->rdma.num_rem_sge) {
				buf->rdma.cur_rem_sge++;
				rseg_length =
					le32_to_cpu(buf->rdma.cur_rem_sge->length);
				rkey  = le32_to_cpu(buf->rdma.cur_rem_sge->lkey);
				buf->rdma.cur_rem_off = 0;
			} else {
				WARN();
				return PTL_FAIL;
			}
		}
	}

	if (debug)
		printf("RDMA posted, resid(%d), rdma_comp(%d)\n",
			   (int) *resid, atomic_read(&buf->rdma.rdma_comp));

	return PTL_OK;
}

struct transport transport_rdma = {
	.type = CONN_TYPE_RDMA,

	.post_tgt_dma = post_tgt_rdma,
	.send_message = send_message_rdma,
};

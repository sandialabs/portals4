/*
 * ptl_rdma.c - RDMA operations
 */
#include "ptl_loc.h"

/*
 * rdma read
 *	build and post a single RDMA read work request
 *	XXX Todo: Consider batching workrequest option as enhancement
 */
int rdma_read(buf_t *rdma_buf, uint64_t raddr, uint32_t rkey,
	struct ibv_sge *loc_sge, int num_loc_sge, uint8_t comp)
{
	int err;
	ni_t *ni = to_ni(rdma_buf);
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;

	if (debug) {
		printf("send RDMA message, buf(%p)\n", rdma_buf);
		printf("radd(%lx), loc_sge->addr(%lx), num_sge(%d)\n",
			raddr, loc_sge[0].addr, num_loc_sge);
	}

	if (num_loc_sge > MAX_INLINE_SGE)
		return PTL_FAIL;

	/* XXXX Todo: handle throttle case and return EBUSY */

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

	err = ibv_post_send(ni->qp, &wr, &bad_wr);
	if (err) {
		WARN();
		return PTL_FAIL;
	}
	pthread_spin_lock(&ni->send_list_lock);
	list_add_tail(&rdma_buf->list, &ni->send_list);
	pthread_spin_unlock(&ni->send_list_lock);

	return PTL_OK;
}

/*
 * rdma_write 
 *	build and post a single RDMA wrwite work request
 *	XXX Todo: Consider batching workrequest option as enhancement
 */
int rdma_write(buf_t *rdma_buf, uint64_t raddr, uint32_t rkey,
	struct ibv_sge *loc_sge, int num_loc_sge, uint32_t imm_data,
	uint8_t imm, uint8_t comp)
{
	int err;
	ni_t *ni = to_ni(rdma_buf);
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;

	if (debug) {
		printf("post RDMA work, buf(%p)\n", rdma_buf);
		printf("radd(%lx), loc_sge->addr(%lx), num_sge(%d)\n",
			raddr, loc_sge[0].addr, num_loc_sge);
	}

	if (num_loc_sge > MAX_INLINE_SGE)
		return PTL_FAIL;

	/* XXXX Todo: handle throttle case and return EBUSY */

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

	err = ibv_post_send(ni->qp, &wr, &bad_wr);
	if (err) {
		WARN();
		return PTL_FAIL;
	}
	pthread_spin_lock(&ni->send_list_lock);
	list_add_tail(&rdma_buf->list, &ni->send_list);
	pthread_spin_unlock(&ni->send_list_lock);

	return PTL_OK;
}

/*
 * build_rdma_sge
 *	Build the local scatter gather list for a target RDMA operation.
 *
 * Returns the number of bytes to be transferred by the SG list.
 */
ptl_size_t build_rdma_sge(xt_t *xt, ptl_size_t rem_len,
	struct ibv_sge *sge, int num_sge, int *entries, ptl_size_t *loc_index,
	ptl_size_t *loc_off, int max_loc_index)
{
	me_t *me = xt->me;
	mr_t *mr;
	ptl_iovec_t *iov;
	ptl_size_t tot_len = 0;
	ptl_size_t len;
	int i;

	for (i = 0; i < num_sge && tot_len < rem_len; i++, sge++) {
		len = rem_len - tot_len;
		if (me->num_iov) {
			if (*loc_index >= max_loc_index) {
				WARN();
				break;
			}
			iov = ((ptl_iovec_t *)me->start) + *loc_index;
			mr = me->mr_list[*loc_index];
			sge->addr = (uintptr_t)iov->iov_base + *loc_off;
			if (len > iov->iov_len - *loc_off)
				len = iov->iov_len - *loc_off;
			sge->lkey = mr->ibmr->lkey;
			sge->length = len;
	
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
			mr = me->mr;
			sge->addr = (uintptr_t)me->start + *loc_off;

			if (len > mr->ibmr->length - *loc_off)
				len = mr->ibmr->length - *loc_off;
			sge->lkey = mr->ibmr->lkey;
			sge->length = len;

			tot_len += len;
			*loc_off += len;
			if (tot_len < rem_len && *loc_off >= mr->ibmr->length)
				break;
		}

	}
	*entries = i;
	return tot_len;
}

/*
 * post_tgt_rdma
 *	Issue one or more RDMA from target to initiator based on target.
 * transfer state.
 *
 * xt - target transfer context
 * dir - direction of transfer from target perspective
 */
int post_tgt_rdma(xt_t *xt, data_dir_t dir)
{
	uint64_t raddr;
	uint32_t rkey;
	ptl_size_t bytes;
	struct ibv_sge sge[MAX_INLINE_SGE];
	int entries = 0;
	ptl_size_t iov_index = xt->cur_loc_iov_index;
	ptl_size_t iov_off = xt->cur_loc_iov_off;;
	uint32_t rlength;
	uint32_t rseg_length;
	ptl_size_t *resid = dir == DATA_DIR_IN ? &xt->put_resid :
		&xt->get_resid;
	int err;
	int comp = 0;

	rseg_length = be32_to_cpu(xt->cur_rem_sge->length);
	rkey  = be32_to_cpu(xt->cur_rem_sge->lkey);

	while (*resid > 0 && !xt->rdma_comp) {
		raddr = be64_to_cpu(xt->cur_rem_sge->addr) + xt->cur_rem_off;
		rlength = rseg_length - xt->cur_rem_off;

		if (debug)
			printf("raddr(0x%lx), rlen(%d), rkey(0x%x)\n",
			raddr, rlength, rkey);

		bytes = build_rdma_sge(xt, rlength, sge, MAX_INLINE_SGE,
				&entries, &iov_index,  &iov_off,
				xt->le->num_iov);
		if (!bytes) {
			WARN();
			return PTL_FAIL;
		}

		xt->interim_rdma++;
		/* XXX todo: we could throttle WR on a QP bases instead of xt */

		if (*resid == bytes || xt->interim_rdma >= MAX_RDMA_WR_OUT) {
			/* XXX todo: if write w/ immediate fold that in */
			comp = 1;
			xt->interim_rdma = 0;
		}

		if (dir == DATA_DIR_IN)
			err = rdma_read(xt->rdma_buf, raddr, rkey, sge,
				entries, comp);
		else
			err = rdma_write(xt->rdma_buf, raddr, rkey, sge,
				entries, 0, 0, comp);

		if (err) {
			WARN();
			return err;
		}

		if (comp)
			xt->rdma_comp++;

		*resid -= bytes;
		xt->cur_loc_iov_index = iov_index;
		xt->cur_loc_iov_off = iov_off;
		xt->cur_rem_off += bytes;

		if (*resid && xt->cur_rem_off >= rseg_length)
			if (xt->num_rem_sge) {
				xt->cur_rem_sge++;
				rseg_length =
					be32_to_cpu(xt->cur_rem_sge->length);
				rkey  = be32_to_cpu(xt->cur_rem_sge->lkey);
				xt->cur_rem_off = 0;
			} else {
				WARN();
				return PTL_FAIL;
			}
		else 
			xt->cur_rem_off += bytes;
	}

	if (debug)
		printf("RDMA posted, resid(%d), rdma_comp(%d)\n",
			(int) *resid, (int) xt->rdma_comp);

	return PTL_OK;
}

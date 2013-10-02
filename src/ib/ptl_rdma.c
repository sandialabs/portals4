/**
 * @file ptl_rdma.c
 *
 * @brief RDMA transport operations used by target.
 */
#include "ptl_loc.h"

/**
 * @param[in] ni
 * @param[in] conn
 *
 * @return status
 *
 * conn must be locked
 */
static int rdma_init_connect(ni_t *ni, conn_t *conn)
{
    struct rdma_cm_id *cm_id = cm_id;

    assert(conn->transport.type == CONN_TYPE_RDMA);

    if (ni->shutting_down)
        return PTL_FAIL;

    conn_get(conn);

    assert(conn->state == CONN_STATE_DISCONNECTED);
    assert(conn->rdma.cm_id == NULL);

    ptl_info("Initiate connect with %x:%d\n", conn->sin.sin_addr.s_addr,
             conn->sin.sin_port);

    conn->rdma.retry_resolve_addr = 3;
    conn->rdma.retry_resolve_route = 3;
    conn->rdma.retry_connect = 3;

    if (rdma_create_id(ni->iface->cm_channel, &cm_id, conn, RDMA_PS_TCP)) {
        WARN();
        conn_put(conn);
        return PTL_FAIL;
    }

    conn->state = CONN_STATE_RESOLVING_ADDR;
    conn->rdma.cm_id = cm_id;

    if (rdma_resolve_addr
        (cm_id, NULL, (struct sockaddr *)&conn->sin,
         get_param(PTL_RDMA_TIMEOUT))) {
        ptl_warn("rdma_resolve_addr failed %x:%d\n",
                 conn->sin.sin_addr.s_addr, conn->sin.sin_port);
        conn->state = CONN_STATE_DISCONNECTED;
        pthread_cond_broadcast(&conn->move_wait);
        conn->rdma.cm_id = NULL;
        rdma_destroy_id(cm_id);
        conn_put(conn);
        return PTL_FAIL;
    }

    ptl_info("Connection initiated successfully to %x:%d\n",
             conn->sin.sin_addr.s_addr, conn->sin.sin_port);

    return PTL_OK;
}

/**
 * @brief Build and post an send work request to transfer
 *
 * @param[in] buf A buf holding state for the send operation.
 *
 * @return status
 */
static int rdma_send_message(buf_t *buf, int from_init)
{
    int err;
    struct ibv_send_wr *bad_wr;
    struct ibv_send_wr wr;
    struct ibv_sge sg_list;
    conn_t *conn = buf->conn;

    wr.wr_id = (uintptr_t) buf;
    wr.next = NULL;
    wr.sg_list = &sg_list;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;

    if ((buf->event_mask & XX_SIGNALED) ||
        (atomic_inc(&buf->conn->rdma.send_comp_threshold) ==
         get_param(PTL_MAX_SEND_COMP_THRESHOLD)) || (from_init &&
                                                     atomic_read(&conn->
                                                                 rdma.num_req_not_comp)
                                                     >=
                                                     get_param
                                                     (PTL_MAX_SEND_COMP_THRESHOLD)))
    {
        wr.send_flags = IBV_SEND_SIGNALED;
        atomic_set(&buf->conn->rdma.send_comp_threshold, 0);

        /* Keep the buffer from being freed until we get the
         * completion. */
        buf_get(buf);
    } else {
        wr.send_flags = 0;
    }

    if (buf->event_mask & XX_INLINE) {
        wr.send_flags |= IBV_SEND_INLINE;

        if (wr.send_flags == IBV_SEND_INLINE) {
            /* Inline and no completion required: fire and forget. If
             * there is an error, we will get a completion anyway, so
             * we must ignore it. */
            wr.wr_id = 0;
        }
    }

    sg_list.addr = (uintptr_t) buf->internal_data;
    sg_list.lkey = buf->rdma.lkey;
    sg_list.length = buf->length;

    buf->type = BUF_SEND;

    /* Rate limit the initiator. If the IB/RDMA send queue gets full, there
     * wouldn't be any space left to send the ACKs/replies, and we
     * would get a deadlock. */
    if (from_init) {
        int limit = buf->conn->rdma.max_req_avail;

        atomic_inc(&conn->rdma.num_req_posted);
        atomic_inc(&conn->rdma.num_req_not_comp);

        /* If the high water mark is reached, wait until we go back to
         * the low watermark (=1/2 high WM). */
        if (atomic_read(&buf->conn->rdma.num_req_posted) >= limit) {
            limit /= 2;
            while (atomic_read(&buf->conn->rdma.num_req_posted) >= limit) {
                pthread_yield();
                SPINLOCK_BODY();
            }
        }

        if (wr.send_flags & IBV_SEND_SIGNALED) {
            /* Atomically set buf->init_req_completes to the current value of
             * conn->rdma.num_req_posted and set
             * conn->rdma.num_req_posted to 0. */
            buf->transfer.rdma.num_req_completes =
                atomic_swap(&conn->rdma.num_req_not_comp, 0);
        }
    }

    err = ibv_post_send(buf->dest.rdma.qp, &wr, &bad_wr);
    if (err) {
        WARN();

        return PTL_FAIL;
    }

    return PTL_OK;
}

static void rdma_set_send_flags(buf_t *buf, int can_signal)
{
    /* If the buffer fits in the work request inline data, then we can
     * inline, in which case the data will be copied during
     * ibv_post_send, else we can't and must wait for the data to be
     * sent before disposing of the buffer. */
    if (buf->length <= buf->conn->rdma.max_inline_data) {
        buf->event_mask |= XX_INLINE;
    } else {
        if (can_signal)
            buf->event_mask |= XX_SIGNALED;
    }
}

/**
 * @brief Build and post an RDMA read/write work request to transfer
 * data to/from one or more local memory segments from/to a single remote
 * memory segment.
 *
 * @param[in] buf A buf holding state for the rdma operation.
 * @param[in] qp The InfiniBand QP to send the rdma to.
 * @param[in] dir The rdma direction in or out.
 * @param[in] raddr The remote address at the initiator.
 * @param[in] rkey The rkey of the InfiniBand MR that registers the
 * memory region at the initiator that includes the data segment.
 * @param[in] sg_list The scatter/gather list that contains
 * the local addresses, lengths and lkeys.
 * @param[in] num_sge The size of the scatter/gather array.
 * @param[in] comp A flag indicating whether to generate a completion
 * event when this operation is complete.
 *
 * @return status
 */
static int post_rdma(buf_t *buf, struct ibv_qp *qp, data_dir_t dir,
                     uint64_t raddr, uint32_t rkey, struct ibv_sge *sg_list,
                     int num_sge)
{
    int err;
    struct ibv_send_wr wr;
    struct ibv_send_wr *bad_wr;

    /* build an infiniband rdma write work request */
    if (likely(buf->event_mask & XX_SIGNALED)) {
        wr.wr_id = (uintptr_t) buf;
        wr.send_flags = IBV_SEND_SIGNALED;
    } else {
        wr.wr_id = 0;
        if (atomic_inc(&buf->conn->rdma.rdma_comp_threshold) ==
            get_param(PTL_MAX_SEND_COMP_THRESHOLD)) {
            wr.send_flags = IBV_SEND_SIGNALED;
            atomic_set(&buf->conn->rdma.rdma_comp_threshold, 0);
        } else {
            wr.send_flags = 0;
        }
    }

    wr.next = NULL;
    wr.sg_list = sg_list;
    wr.num_sge = num_sge;
    wr.opcode = (dir == DATA_DIR_IN) ? IBV_WR_RDMA_READ : IBV_WR_RDMA_WRITE;
    wr.wr.rdma.remote_addr = raddr;
    wr.wr.rdma.rkey = rkey;

    /* post the work request to the QP send queue for the
     * destination/initiator */
    err = ibv_post_send(buf->dest.rdma.qp, &wr, &bad_wr);
    if (err) {
        WARN();
        return PTL_FAIL;
    }

    return PTL_OK;
}

static void append_init_data_rdma_direct(data_t *data, mr_t *mr, void *addr,
                                         ptl_size_t length, buf_t *buf)
{
    data->data_fmt = DATA_FMT_RDMA_DMA;
    data->rdma.num_sge = cpu_to_le32(1);
    data->rdma.sge_list[0].addr =
        cpu_to_le64((uintptr_t) addr_to_ppe(addr, mr));
    data->rdma.sge_list[0].length = cpu_to_le32(length);
    data->rdma.sge_list[0].lkey = cpu_to_le32(mr->ibmr->rkey);

    buf->length += sizeof(*data) + sizeof(struct ibv_sge);
}

static void append_init_data_rdma_iovec_direct(data_t *data, md_t *md,
                                               int iov_start, int num_iov,
                                               ptl_size_t length, buf_t *buf)
{
    data->data_fmt = DATA_FMT_RDMA_DMA;
    data->rdma.num_sge = cpu_to_le32(num_iov);
    memcpy(data->rdma.sge_list, &md->sge_list[iov_start],
           num_iov * sizeof(struct ibv_sge));

    buf->length += sizeof(*data) + num_iov * sizeof(struct ibv_sge);
}

static void append_init_data_rdma_iovec_indirect(data_t *data, md_t *md,
                                                 int iov_start, int num_iov,
                                                 ptl_size_t length,
                                                 buf_t *buf)
{
    data->data_fmt = DATA_FMT_RDMA_INDIRECT;
    data->rdma.num_sge = cpu_to_le32(1);
    data->rdma.sge_list[0].addr = cpu_to_le64((uintptr_t)
                                              addr_to_ppe(&md->
                                                          sge_list[iov_start],
                                                          md->ppe.mr_start));
    data->rdma.sge_list[0].length =
        cpu_to_le32(num_iov * sizeof(struct ibv_sge));
    data->rdma.sge_list[0].lkey = cpu_to_le32(md->sge_list_mr->ibmr->rkey);

    buf->length += sizeof(*data) + sizeof(struct ibv_sge);
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
static int rdma_init_prepare_transfer(md_t *md, data_dir_t dir,
                                      ptl_size_t offset, ptl_size_t length,
                                      buf_t *buf)
{
    int err = PTL_OK;
    req_hdr_t *hdr = (req_hdr_t *) buf->data;
    data_t *data = (data_t *)(buf->data + buf->length);
    int num_sge;
    ptl_size_t iov_start = 0;
    ptl_size_t iov_offset = 0;

    if (length <= get_param(PTL_MAX_INLINE_DATA)) {
        mr_t *mr;
        if (md->num_iov) {
            err =
                append_immediate_data(md->start, md->mr_list, md->num_iov,
                                      dir, offset, length, buf);
        } else {
            err =
                mr_lookup_app(obj_to_ni(md), md->start + offset, length, &mr);
            if (err) {
                WARN();
                return PTL_FAIL;
            }

            err =
                append_immediate_data(md->start, &mr, md->num_iov, dir,
                                      offset, length, buf);

            mr_put(mr);
        }
    } else if (md->options & PTL_IOVEC) {
        ptl_iovec_t *iovecs = md->start;

        /* Find the index and offset of the first IOV as well as the
         * total number of IOVs to transfer. */
        num_sge =
            iov_count_elem(iovecs, md->num_iov, offset, length, &iov_start,
                           &iov_offset);
        if (num_sge < 0) {
            WARN();
            return PTL_FAIL;
        }

        if (num_sge > get_param(PTL_MAX_INLINE_SGE))
            /* Indirect case. The IOVs do not fit in a buf_t. */
            append_init_data_rdma_iovec_indirect(data, md, iov_start, num_sge,
                                                 length, buf);
        else
            append_init_data_rdma_iovec_direct(data, md, iov_start, num_sge,
                                               length, buf);

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

            append_init_data_rdma_direct(data, mr, addr, length, buf);
        }
    }

    if (!err)
        assert(buf->length <= BUF_DATA_SIZE);

    return err;
}

static int rdma_tgt_data_out(buf_t *buf, data_t *data)
{
    int next;

    switch (data->data_fmt) {
        case DATA_FMT_RDMA_DMA:
            /* Read from SG list provided directly in request */
            buf->transfer.rdma.cur_rem_sge = &data->rdma.sge_list[0];
            buf->transfer.rdma.cur_rem_off = 0;
            buf->transfer.rdma.num_rem_sge = le32_to_cpu(data->rdma.num_sge);

            next = STATE_TGT_RDMA;
            break;

        case DATA_FMT_RDMA_INDIRECT:
            next = STATE_TGT_WAIT_RDMA_DESC;
            break;

        default:
            assert(0);
            WARN();
            next = STATE_TGT_ERROR;
    }

    return next;
}

/**
 * @brief Build the local scatter gather list for a target RDMA operation.
 *
 * The most general case is transfering from an iovec to an iovec.
 * This requires a double loop iterating over the memory segments
 * at the (remote) initiator and also over the memory segments in the
 * (local) target list element. This routine implements the loop over
 * the local memory segments building an InfiniBand scatter/gather
 * array to be used in an rdma operation. It is called by rdma_process_transfer
 * below which implements the outer loop over the remote memory segments.
 * The case where one or both the MD and the LE/ME do not have an iovec
 * are handled as limits of the general case.
 *
 * @param[in] buf The message buffer received by the target.
 * @param[in,out] cur_index_p The current index in the LE/ME iovec.
 * @param[in,out] cur_off_p The offset into the current LE/ME
 * iovec element.
 * @param[in] sge The scatter/gather array to fill in.
 * @param[in] sge_size The size of the scatter/gather array.
 * @param[out] num_sge_p The number of sge entries used.
 * @param[out] mr_list A list of MRs used to map the local memory segments.
 * @param[in,out] length_p On input the requested number of bytes to be
 * transfered in the current rdma operation. On exit the actual number
 * of bytes transfered.
 *
 * @return status
 */
static int build_sge(buf_t *buf, ptl_size_t *cur_index_p,
                     ptl_size_t *cur_off_p, struct ibv_sge *sge, int sge_size,
                     int *num_sge_p, mr_t **mr_list, ptl_size_t *length_p)
{
    int err;
    ni_t *ni = obj_to_ni(buf);
    me_t *me = buf->me;
    mr_t *mr;
    ptl_iovec_t *iov = iov;
    ptl_size_t bytes;
    ptl_size_t cur_index = *cur_index_p;
    ptl_size_t cur_off = *cur_off_p;
    ptl_size_t cur_len = 0;
    int num_sge = 0;
    void *addr;
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
        err = mr_lookup_app(ni, addr, bytes, &mr);
        if (err)
            return err;

        sge->addr = (uintptr_t) addr_to_ppe(addr, mr);
        sge->length = bytes;
        sge->lkey = mr->ibmr->lkey;

        /* save the mr and the reference to it until
         * we receive a completion */
        *mr_list++ = mr;

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
 * @brief Allocate a temporary buf to hold mr references for an rdma operation.
 *
 * @param[in] buf The message buf.
 *
 * @return the buf
 */
static buf_t *tgt_alloc_rdma_buf(buf_t *buf)
{
    buf_t *rdma_buf;
    int err;

    err = buf_alloc(obj_to_ni(buf), &rdma_buf);
    if (err) {
        WARN();
        return NULL;
    }

    rdma_buf->type = BUF_RDMA;
    rdma_buf->transfer.rdma.xxbuf = buf;
    buf_get(buf);
    rdma_buf->dest = buf->dest;
    rdma_buf->conn = buf->conn;

    return rdma_buf;
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
static int rdma_do_transfer(buf_t *buf)
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
    int sge_size = buf->obj.obj_ni->iface->cap.max_send_sge;
    struct ibv_sge sge_list[sge_size];
    int entries = 0;
    int cur_rdma_ops = 0;
    size_t rem_off;
    uint32_t rem_size;
    struct ibv_sge *rem_sge;
    uint32_t rem_key;
    int max_rdma_ops = get_param(PTL_MAX_RDMA_WR_OUT);
    mr_t **mr_list;

    dir = buf->rdma_dir;
    resid = (dir == DATA_DIR_IN) ? buf->put_resid : buf->get_resid;
    iov_index = buf->cur_loc_iov_index;
    iov_off = buf->cur_loc_iov_off;

    rem_sge = buf->transfer.rdma.cur_rem_sge;
    rem_off = buf->transfer.rdma.cur_rem_off;
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

        mr_list = rdma_buf->mr_list + rdma_buf->num_mr;

        /* build a local scatter/gather array on our stack
         * to transfer as many bytes as possible from the
         * LE/ME up to rlength. The transfer size may be
         * limited by the size of the scatter/gather list
         * sge_list. The new values of iov_index and iov_offset
         * are returned as well as the number of bytes
         * transferred. */
        err =
            build_sge(buf, &iov_index, &iov_off, sge_list, sge_size, &entries,
                      mr_list, &bytes);
        if (err) {
            buf_put(rdma_buf);
            return err;
        }

        rdma_buf->num_mr += entries;

        /* add the rdma_buf to a list of pending rdma
         * transfers at the buf. These will get
         * cleaned up in tgt_cleanup. The mr's will
         * get dropped in buf_cleanup */
        PTL_FASTLOCK_LOCK(&buf->rdma.rdma_list_lock);
        list_add_tail(&rdma_buf->list, &buf->transfer.rdma.rdma_list);
        PTL_FASTLOCK_UNLOCK(&buf->rdma.rdma_list_lock);

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

        if (comp)
            rdma_buf->event_mask |= XX_SIGNALED;

        /* post the rdma read or write operation to the QP */
        err =
            post_rdma(rdma_buf, buf->dest.rdma.qp, dir, addr, rem_key,
                      sge_list, entries);
        if (err) {
            PTL_FASTLOCK_LOCK(&buf->rdma.rdma_list_lock);
            list_del(&rdma_buf->list);
            PTL_FASTLOCK_UNLOCK(&buf->rdma.rdma_list_lock);
            return err;
        }

        if (comp)
            break;
    }

    /* update the current rdma state */
    buf->cur_loc_iov_index = iov_index;
    buf->cur_loc_iov_off = iov_off;
    buf->transfer.rdma.cur_rem_off = rem_off;
    buf->transfer.rdma.cur_rem_sge = rem_sge;

    if (dir == DATA_DIR_IN)
        buf->put_resid = resid;
    else
        buf->get_resid = resid;

    return PTL_OK;
}

struct transport transport_rdma = {
    .type = CONN_TYPE_RDMA,
    .buf_alloc = buf_alloc,
    .init_connect = rdma_init_connect,
    .send_message = rdma_send_message,
    .set_send_flags = rdma_set_send_flags,
    .init_prepare_transfer = rdma_init_prepare_transfer,
    .post_tgt_dma = rdma_do_transfer,
    .tgt_data_out = rdma_tgt_data_out,
};

/* Wait for all to be disconnected. RDMA CM is handling disconnection
 * timeouts, so we should never block forever because of this test. */
static int is_disconnected_all(ni_t *ni)
{
    return atomic_read(&ni->rdma.num_conn) == 0;
}

struct transport_ops transport_remote_rdma = {
    .init_iface = init_iface_rdma,
    .NIInit = PtlNIInit_rdma,
    .NIFini = cleanup_rdma,
    .initiate_disconnect_all = initiate_disconnect_all,
    .is_disconnected_all = is_disconnected_all,
};

/**
 * @brief Request the indirect scatter/gather list.
 *
 * @param[in] buf The message buffer received by the target.
 *
 * @return status
 */
int process_rdma_desc(buf_t *buf)
{
    int err;
    ni_t *ni = obj_to_ni(buf);
    data_t *data;
    uint64_t raddr;
    uint32_t rkey;
    uint32_t rlen;
    struct ibv_sge sge;
    int num_sge;
    void *indir_sge;
    mr_t *mr;

    data = buf->rdma_dir == DATA_DIR_IN ? buf->data_in : buf->data_out;

    raddr = le64_to_cpu(data->rdma.sge_list[0].addr);
    rkey = le32_to_cpu(data->rdma.sge_list[0].lkey);
    rlen = le32_to_cpu(data->rdma.sge_list[0].length);

    indir_sge = malloc(rlen);
    if (!indir_sge) {
        WARN();
        err = PTL_FAIL;
        goto err1;
    }

    if (mr_lookup_self(ni, indir_sge, rlen, &mr)) {
        WARN();
        err = PTL_FAIL;
        goto err1;
    }

    buf->indir_sge = indir_sge;
    buf->mr_list[buf->num_mr++] = mr;

    sge.addr = (uintptr_t) indir_sge;
    sge.lkey = mr->ibmr->lkey;
    sge.length = rlen;

    num_sge = 1;

    /* use the buf as its own rdma buf. */
    buf->event_mask |= XX_SIGNALED;
    buf->transfer.rdma.xxbuf = buf;
    buf->type = BUF_RDMA;

    err =
        post_rdma(buf, buf->dest.rdma.qp, DATA_DIR_IN, raddr, rkey, &sge,
                  num_sge);
    if (err) {
        err = PTL_FAIL;
        goto err1;
    }

    err = PTL_OK;
  err1:
    return err;
}

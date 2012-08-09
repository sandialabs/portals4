/**
 * @file ptl_shmem.c
 */

#include "ptl_loc.h"

/**
 * @brief Send a message using UDP.
 *
 * @param[in] buf
 * @param[in] signaled
 *
 * @return status
 */
static int send_message_udp(buf_t *buf, int from_init)
{
	ssize_t ret;

	/* Keep a reference on the buffer so it doesn't get freed. */
	assert(buf->obj.obj_pool->type == POOL_BUF);
	buf_get(buf);

	ret = sendto(buf->dest.udp.s, buf->data, buf->length, 0,
				 buf->dest.udp.dest_addr, sizeof(struct sockaddr_in));

	assert(ret == buf->length);

	return PTL_OK;
}

static void udp_set_send_flags(buf_t *buf, int can_signal)
{
	/* The data is always in the buffer. */
	buf->event_mask |= XX_INLINE;
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
static int init_prepare_transfer_udp(md_t *md, data_dir_t dir, ptl_size_t offset,
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
		if (md->options & PTL_IOVEC) {
#if 0
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
#endif
			abort();//todo
		} else {
#if 0
			void *addr;
			mr_t *mr;
			ni_t *ni = obj_to_ni(md);

			addr = md->start + offset;
			err = mr_lookup_app(ni, addr, length, &mr);
			if (!err) {
				buf->mr_list[buf->num_mr++] = mr;

				append_init_data_udp_direct(data, mr, addr, length, buf);
			}

#endif
		abort();//todo

		}
	}

	if (!err)
		assert(buf->length <= BUF_DATA_SIZE);

	return err;
}

/**
 * @param[in] ni
 * @param[in] conn
 *
 * @return status
 *
 * conn must be locked
 */
static int init_connect_udp(ni_t *ni, conn_t *conn)
{
	int ret;

	assert(conn->transport.type == CONN_TYPE_UDP);

	if (ni->shutting_down)
		return PTL_FAIL;

	conn_get(conn);

	assert(conn->state == CONN_STATE_DISCONNECTED);

	/* Send the connect request. */
	struct udp_conn_msg msg;
	msg.msg_type = cpu_to_le16(UDP_CONN_MSG_REQ);
	msg.port = cpu_to_le16(ni->udp.src_port);
	msg.req.options = ni->options;
	msg.req.src_id = ni->id;
	msg.req_cookie = (uintptr_t)conn;

	/* Send the request to the listening socket on the remote node. */
	ret = sendto(ni->iface->udp.connect_s, &msg, sizeof(msg), 0,
				 &conn->sin, sizeof(conn->sin));
	if (ret == -1) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

struct transport transport_udp = {
	.type = CONN_TYPE_UDP,
	.buf_alloc = buf_alloc,
	.init_connect = init_connect_udp,
	.send_message = send_message_udp,
	.set_send_flags = udp_set_send_flags,
	.init_prepare_transfer = init_prepare_transfer_udp,
	//	.post_tgt_dma = do_noknem_transfer,
	//	.tgt_data_out = noknem_tgt_data_out,
};

struct transport_ops transport_remote_udp = {
	.init_iface = init_iface_udp,
	.NIInit = PtlNIInit_UDP,
};

/**
 * @file ptl_mem.c
 *
 * Local copy operations
 */

#include "ptl_loc.h"

#if (WITH_TRANSPORT_SHMEM && USE_KNEM) || IS_PPE
ptl_size_t copy_mem_to_mem(ni_t *ni, data_dir_t dir, struct mem_iovec *remote_iovec, void *local_addr, mr_t *local_mr, ptl_size_t len)
{
	ptl_size_t copied;

#if (WITH_TRANSPORT_SHMEM && USE_KNEM)
	if (dir == DATA_DIR_IN)
		copied = knem_copy(ni, remote_iovec->cookie, remote_iovec->offset, 
						local_mr->knem_cookie,
						local_addr - local_mr->addr, len);
	else
		copied = knem_copy(ni, local_mr->knem_cookie,
						local_addr - local_mr->addr,
						remote_iovec->cookie, remote_iovec->offset, len);
#elif IS_PPE
	local_addr = addr_to_ppe(local_addr, local_mr);
	if (dir == DATA_DIR_IN)
		memcpy(local_addr, remote_iovec->addr, len);
	else
		memcpy(remote_iovec->addr, local_addr, len);
	copied = len;
#endif

	return copied;
}

static inline void advance_remote_addr(struct mem_iovec *remote_iovec, ptl_size_t len)
{
#if (WITH_TRANSPORT_SHMEM && USE_KNEM)
	remote_iovec->offset += len;
#elif IS_PPE
	remote_iovec->addr += len;
#endif	
}

/**
 * @brief Do a shared memory copy using the knem device.
 *
 * @param[in] buf
 * @param[in] rem_len
 * @param[in] iovec
 * @param[in,out] loc_index
 * @param[in,out] loc_off
 * @param[in] max_loc_index
 * @param[in] dir
 *
 * @return the number of bytes to be transferred by the SG list.
 */
static ptl_size_t do_mem_copy(buf_t *buf, ptl_size_t rem_len,
							  struct mem_iovec *iovec,
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

			iov = ((ptl_iovec_t *)addr_to_ppe(me->start, me->mr_start)) + *loc_index;

			addr = iov->iov_base + *loc_off;

			if (len > iov->iov_len - *loc_off)
				len = iov->iov_len - *loc_off;

			err = mr_lookup_app(obj_to_ni(buf), addr, len, &mr);
			if (err)
				break;

			copy_mem_to_mem(ni, dir, iovec, addr, mr, len);
			advance_remote_addr(iovec, len);

			mr_put(mr);

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
			void *addr;
			ptl_size_t len_available = me->length - *loc_off;

			assert(me->length > *loc_off);
				
			if (len > len_available)
				len = len_available;

			addr = me->start + *loc_off;

			err = mr_lookup_app(ni, addr, len, &mr);
			if (err)
				break;

			copy_mem_to_mem(ni, dir, iovec, addr, mr, len);
			advance_remote_addr(iovec, len);

			mr_put(mr);

			tot_len += len;
			*loc_off += len;
			if (tot_len < rem_len && *loc_off >= mr->length)
				break;
		}
	}

	return tot_len;
}

/**
 * @brief Copy from one buffer to another, including from and/or to iovecs.
 *
 * @param[in] buf
 *
 * @return status
 */
int do_mem_transfer(buf_t *buf)
{
	ptl_size_t bytes;
	ptl_size_t iov_index = buf->cur_loc_iov_index;
	ptl_size_t iov_off = buf->cur_loc_iov_off;
	data_dir_t dir = buf->rdma_dir;
	ptl_size_t *resid = (dir == DATA_DIR_IN) ?
				&buf->put_resid : &buf->get_resid;
	struct mem_iovec iovec;

	iovec = *buf->transfer.mem.cur_rem_iovec;

	while (*resid > 0) {

#ifdef WITH_TRANSPORT_SHMEM
		iovec.offset += buf->transfer.mem.cur_rem_off;
#endif
		iovec.length -= buf->transfer.mem.cur_rem_off;

		if (iovec.length > *resid)
			iovec.length = *resid;

		bytes = do_mem_copy(buf, iovec.length, &iovec,
							&iov_index, &iov_off, buf->le->num_iov,
							dir);
		if (!bytes)
			return PTL_FAIL;

		*resid -= bytes;
		buf->cur_loc_iov_index = iov_index;
		buf->cur_loc_iov_off = iov_off;
		buf->transfer.mem.cur_rem_off += bytes;

		if (*resid && buf->transfer.mem.cur_rem_off >= iovec.length) {
			if (buf->transfer.mem.num_rem_iovecs) {
				buf->transfer.mem.cur_rem_iovec++;
				iovec = *buf->transfer.mem.cur_rem_iovec;
				buf->transfer.mem.cur_rem_off = 0;
			} else {
				return PTL_FAIL;
			}
		}
	}

	return PTL_OK;
}
#endif

/* Computes a hash (crc32 based), to identify which group this NI belong to. */
static uint32_t crc32(const unsigned char *p, uint32_t crc, int size)
{
    while (size--) {
		int n;

        crc ^= *p++;
        for (n = 0; n < 8; n++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }

    return crc;
}

/* Lookup our nid/pid to determine local rank */
void PtlSetMap_mem(ni_t *ni,
				   ptl_size_t map_size,
				   const ptl_process_t *mapping)
{
	iface_t *iface = ni->iface;
	int i;

	ni->mem.node_size = 0;
	ni->mem.index = -1;
	ni->mem.hash = ni->options;

	for (i = 0; i < map_size; i++) {
		if (mapping[i].phys.nid == iface->id.phys.nid) {
			conn_t *conn;
			ptl_process_t id;

			if (mapping[i].phys.pid == iface->id.phys.pid) {
				/* Self. */
				ni->mem.index = ni->mem.node_size;
			}

			/* Connect local ranks through XPMEM or SHMEM. */
			id.rank = i;
			conn = get_conn(ni, id);
			if (!conn) {
				/* It's hard to recover from here. */
				WARN();
				abort();
				return;
			}

#if IS_PPE
			conn->transport = transport_mem;
#elif WITH_TRANSPORT_SHMEM
			conn->transport = transport_shmem;
			conn->shmem.local_rank = i;
#else
#error
#endif
			conn->state = CONN_STATE_CONNECTED;

			conn_put(conn);			/* from get_conn */
			
			ni->mem.node_size ++;
		}

		ni->mem.hash = crc32((unsigned char *)&mapping[i].phys, ni->mem.hash, sizeof(mapping[i].phys));
	}
}

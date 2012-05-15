/**
 * @file ptl_mem.c
 *
 * Local copy operations
 */

#include "ptl_loc.h"

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
static ptl_size_t do_mem_copy(buf_t *buf, ptl_size_t rem_len,
			       const struct mem_iovec *iovec, uint64_t roffset,
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
				err = knem_copy(ni, iovec->cookie, roffset, 
						mr->knem_cookie,
						addr - mr->addr, len);
			else
				err = knem_copy(ni, mr->knem_cookie,
						addr - mr->addr,
						iovec->cookie, roffset, len);

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
				knem_copy(ni, iovec->cookie, roffset, 
					  mr->knem_cookie,
					  addr - mr->addr, len);
			else
				knem_copy(ni, mr->knem_cookie,
					  addr - mr->addr, iovec->cookie,
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
int do_mem_transfer(buf_t *buf)
{
	uint64_t roffset;
	ptl_size_t bytes;
	ptl_size_t iov_index = buf->cur_loc_iov_index;
	ptl_size_t iov_off = buf->cur_loc_iov_off;
	uint32_t rlength;
	data_dir_t dir = buf->rdma_dir;
	ptl_size_t *resid = (dir == DATA_DIR_IN) ?
				&buf->put_resid : &buf->get_resid;
	struct mem_iovec *iovec;

	iovec = buf->transfer.mem.cur_rem_iovec;
	roffset = iovec->offset;

	while (*resid > 0) {

		roffset += buf->transfer.mem.cur_rem_off;
		rlength = iovec->length - buf->transfer.mem.cur_rem_off;

		if (rlength > *resid)
			rlength = *resid;

		bytes = do_mem_copy(buf, rlength, iovec, roffset,
							&iov_index, &iov_off, buf->le->num_iov,
							dir);
		if (!bytes)
			return PTL_FAIL;

		*resid -= bytes;
		buf->cur_loc_iov_index = iov_index;
		buf->cur_loc_iov_off = iov_off;
		buf->transfer.mem.cur_rem_off += bytes;

		if (*resid && buf->transfer.mem.cur_rem_off >= iovec->length) {
			if (buf->transfer.mem.num_rem_iovecs) {
				buf->transfer.mem.cur_rem_iovec++;
				iovec = buf->transfer.mem.cur_rem_iovec;
				roffset = iovec->offset;
				buf->transfer.mem.cur_rem_off = 0;
			} else {
				return PTL_FAIL;
			}
		}
	}

	return PTL_OK;
}

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
			if (mapping[i].phys.pid == iface->id.phys.pid) {
				ni->mem.index = ni->mem.node_size;
			}

			ni->mem.node_size ++;
		}

		ni->mem.hash = crc32((unsigned char *)&mapping[i].phys, ni->mem.hash, sizeof(mapping[i].phys));
	}
}

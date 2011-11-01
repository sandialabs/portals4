/*
 * ptl_hdr.c - packet header utilities
 */

#include "ptl_loc.h"

void xport_hdr_from_buf(hdr_t *hdr, buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	req_hdr_t *req_hdr = (req_hdr_t *)buf->data;

	hdr->version = PTL_HDR_VER_1;
	hdr->atom_op = req_hdr->atom_op;
	hdr->atom_type = req_hdr->atom_type;
	hdr->ni_type = ni->ni_type;
	hdr->src_nid = cpu_to_le32(ni->id.phys.nid);
	hdr->src_pid = cpu_to_le32(ni->id.phys.pid);

	hdr->ni_fail = buf->ni_fail;
	hdr->pkt_fmt = PKT_FMT_REPLY;
	hdr->dst_nid = req_hdr->src_nid;
	hdr->dst_pid = req_hdr->src_pid;
	hdr->hdr_size = sizeof(hdr_t);
}

void base_hdr_from_buf(hdr_t *hdr, buf_t *buf)
{
	req_hdr_t *req_hdr = (req_hdr_t *)buf->data;

	hdr->length		= cpu_to_le64(buf->mlength);
	hdr->offset		= cpu_to_le64(buf->moffset);
	hdr->handle		= req_hdr->handle;
}

/*
 * ptl_hdr.c - packet header utilities
 */

#include "ptl_loc.h"

void xport_hdr_from_xt(hdr_t *hdr, xt_t *xt)
{
	ni_t *ni = obj_to_ni(xt);

	hdr->version = PTL_HDR_VER_1;
	hdr->atom_op = xt->atom_op;
	hdr->atom_type = xt->atom_type;
	hdr->ni_type = ni->ni_type;
	hdr->src_nid = cpu_to_le32(ni->id.phys.nid);
	hdr->src_pid = cpu_to_le32(ni->id.phys.pid);

	hdr->ni_fail = xt->ni_fail;
	hdr->pkt_fmt = PKT_FMT_REPLY;
	hdr->dst_nid = cpu_to_le32(xt->initiator.phys.nid);
	hdr->dst_pid = cpu_to_le32(xt->initiator.phys.pid);
	hdr->hdr_size = sizeof(hdr_t);
}

void xport_hdr_to_xt(hdr_t *hdr, xt_t *xt)
{
	xt->atom_op = hdr->atom_op;
	xt->atom_type = hdr->atom_type;
	xt->ack_req = hdr->ack_req;
	xt->initiator.phys.nid = le32_to_cpu(hdr->src_nid);
	xt->initiator.phys.pid = le32_to_cpu(hdr->src_pid);
}

void base_hdr_from_xt(hdr_t *hdr, xt_t *xt)
{
	hdr->length		= cpu_to_le64(xt->mlength);
	hdr->offset		= cpu_to_le64(xt->moffset);
	hdr->handle		= cpu_to_le64(xt->xi_handle);
}

void base_hdr_to_xt(hdr_t *hdr, xt_t *xt)
{
	xt->rlength		= le64_to_cpu(hdr->length);
	xt->roffset		= le64_to_cpu(hdr->offset);
	xt->xi_handle		= le64_to_cpu(hdr->handle);
}

void req_hdr_to_xt(req_hdr_t *hdr, xt_t *xt)
{
	xt->match_bits = le64_to_cpu(hdr->match_bits);
	xt->hdr_data = le64_to_cpu(hdr->hdr_data);
	xt->operand = le64_to_cpu(hdr->operand);
	xt->pt_index = le32_to_cpu(hdr->pt_index);
	xt->uid = le32_to_cpu(hdr->uid);
}

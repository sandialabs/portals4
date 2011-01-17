/*
 * ptl_hdr.c - packet header utilities
 */

#include "ptl_loc.h"

static void xport_hdr_from_xx(hdr_t *hdr, xi_t *xi)
{
	ni_t *ni = to_ni(xi);

	hdr->version = PTL_HDR_VER_1;
	hdr->atom_op = xi->atom_op;
	hdr->atom_type = xi->atom_type;
	hdr->ni_type = ni->ni_type;
	hdr->src_nid = ni->nid;
	hdr->src_pid = ni->pid;
}

void xport_hdr_from_xi(hdr_t *hdr, xi_t *xi)
{
	xport_hdr_from_xx(hdr, xi);

	hdr->ack_req = xi->ack_req;
	hdr->pkt_fmt = PKT_FMT_REQ;
	hdr->dst_nid = cpu_to_be32(xi->target.phys.nid);
	hdr->dst_pid = cpu_to_be32(xi->target.phys.pid);
}

void xport_hdr_from_xt(hdr_t *hdr, xt_t *xt)
{
	xport_hdr_from_xx(hdr, (xi_t *)xt);

	hdr->pkt_fmt = PKT_FMT_REPLY;
	hdr->dst_nid = cpu_to_be32(xt->initiator.phys.nid);
	hdr->dst_pid = cpu_to_be32(xt->initiator.phys.pid);
}

void xport_hdr_to_xi(hdr_t *hdr, xi_t *xi)
{
	xi->atom_op = hdr->atom_op;
	xi->atom_type = hdr->atom_type;
	xi->ack_req = hdr->ack_req;
}

void xport_hdr_to_xt(hdr_t *hdr, xt_t *xt)
{
	xt->atom_op = hdr->atom_op;
	xt->atom_type = hdr->atom_type;
	xt->ack_req = hdr->ack_req;
	xt->initiator.phys.nid = be32_to_cpu(hdr->src_nid);
	xt->initiator.phys.pid = be32_to_cpu(hdr->src_pid);
}

void base_hdr_from_xi(hdr_t *hdr, xi_t *xi)
{
	hdr->length		= cpu_to_be64(xi->rlength);
	hdr->offset		= cpu_to_be64(xi->roffset);
	hdr->handle		= cpu_to_be64(xi_to_handle(xi));
}

void base_hdr_from_xt(hdr_t *hdr, xt_t *xt)
{
	hdr->length		= cpu_to_be64(xt->mlength);
	hdr->offset		= cpu_to_be64(xt->moffset);
	hdr->handle		= cpu_to_be64(xt->xi_handle);
}

void base_hdr_to_xi(hdr_t *hdr, xi_t *xi)
{
	xi->mlength		= be64_to_cpu(hdr->length);
	xi->moffset		= be64_to_cpu(hdr->offset);
	xi->xt_handle		= be64_to_cpu(hdr->handle);
}

void base_hdr_to_xt(hdr_t *hdr, xt_t *xt)
{
	xt->rlength		= be64_to_cpu(hdr->length);
	xt->roffset		= be64_to_cpu(hdr->offset);
	xt->xi_handle		= be64_to_cpu(hdr->handle);
}

void req_hdr_from_xi(req_hdr_t *hdr, xi_t *xi)
{
	hdr->match_bits = cpu_to_be64(xi->match_bits);
	hdr->hdr_data = cpu_to_be64(xi->hdr_data);
	hdr->operand = cpu_to_be64(xi->operand);
	hdr->pt_index = cpu_to_be32(xi->pt_index);
	hdr->jid = cpu_to_be32(xi->jid);
	hdr->uid = cpu_to_be32(xi->uid);
}

void req_hdr_to_xt(req_hdr_t *hdr, xt_t *xt)
{
	xt->match_bits = be64_to_cpu(hdr->match_bits);
	xt->hdr_data = be64_to_cpu(hdr->hdr_data);
	xt->operand = be64_to_cpu(hdr->operand);
	xt->pt_index = be32_to_cpu(hdr->pt_index);
	xt->jid = be32_to_cpu(hdr->jid);
	xt->uid = be32_to_cpu(hdr->uid);
}

/*
 * mask.c - parse portals API bit masks
 */

#include "ptl_test.h"

unsigned int get_ni_opt(char *orig_val)
{
	char *val;
	char *tok;
	char *save;
	unsigned int ni_opt = 0;

	orig_val = strdup(orig_val);
	val = orig_val;

	while((tok = strtok_r(val, " \t", &save))) {
		if (!strcmp("MATCH", tok)) {
			ni_opt |= PTL_NI_MATCHING;
		} else if (!strcmp("NO_MATCH", tok)) {
			ni_opt |= PTL_NI_NO_MATCHING;
		} else if (!strcmp("LOGICAL", tok)) {
			ni_opt |= PTL_NI_LOGICAL;
		} else if (!strcmp("PHYSICAL", tok)) {
			ni_opt |= PTL_NI_PHYSICAL;
		} else if (!strcmp("INVALID", tok)) {
			ni_opt = ~PTL_NI_INIT_OPTIONS_MASK;
		} else {
			ni_opt = strtol(tok, NULL, 0);
			break;
		}
		val = NULL;
	}

	free(orig_val);

	return ni_opt;
}

unsigned int get_pt_opt(char *orig_val)
{
	char *val;
	char *tok;
	char *save;
	unsigned int pt_opt = 0;

	orig_val = strdup(orig_val);
	val = orig_val;

	while((tok = strtok_r(val, " \t", &save))) {
		if (!strcmp("USE_ONCE", tok)) {
			pt_opt |= PTL_PT_ONLY_USE_ONCE;
		} else if (!strcmp("FLOW", tok)) {
			pt_opt |= PTL_PT_FLOWCTRL;
		} else if (!strcmp("INVALID", tok)) {
			pt_opt = ~PTL_PT_ALLOC_OPTIONS_MASK;
		} else {
			pt_opt = strtol(tok, NULL, 0);
			break;
		}
		val = NULL;
	}

	free(orig_val);

	return pt_opt;
}

unsigned int get_md_opt(char *orig_val)
{
	char *val;
	char *tok;
	char *save;
	unsigned int md_opt = 0;

	orig_val = strdup(orig_val);
	val = orig_val;

	while((tok = strtok_r(val, " \t", &save))) {
		if (!strcmp("IOVEC", tok)) {
			md_opt |= PTL_IOVEC;
		} else if (!strcmp("SUCCESS_DISABLE", tok)) {
			md_opt |= PTL_MD_EVENT_SUCCESS_DISABLE;
		} else if (!strcmp("CT_SEND", tok)) {
			md_opt |= PTL_MD_EVENT_CT_SEND;
		} else if (!strcmp("CT_REPLY", tok)) {
			md_opt |= PTL_MD_EVENT_CT_REPLY;
		} else if (!strcmp("CT_ACK", tok)) {
			md_opt |= PTL_MD_EVENT_CT_ACK;
		} else if (!strcmp("CT_BYTES", tok)) {
			md_opt |= PTL_MD_EVENT_CT_BYTES;
		} else if (!strcmp("UNORDERED", tok)) {
			md_opt |= PTL_MD_UNORDERED;
		} else if (!strcmp("INVALID", tok)) {
			md_opt = ~PTL_MD_OPTIONS_MASK;
		} else {
			md_opt = strtol(tok, NULL, 0);
			break;
		}
		val = NULL;
	}

	free(orig_val);

	return md_opt;
}

unsigned int get_le_opt(char *orig_val)
{
	char *val;
	char *tok;
	char *save;
	unsigned int le_opt = 0;

	orig_val = strdup(orig_val);
	val = orig_val;

	while((tok = strtok_r(val, " \t", &save))) {
		if (!strcmp("IOVEC", tok)) {
			le_opt |= PTL_IOVEC;
		} else if (!strcmp("OP_PUT", tok)) {
			le_opt |= PTL_LE_OP_PUT;
		} else if (!strcmp("OP_GET", tok)) {
			le_opt |= PTL_LE_OP_GET;
		} else if (!strcmp("USE_ONCE", tok)) {
			le_opt |= PTL_LE_USE_ONCE;
		} else if (!strcmp("ACK_DISABLE", tok)) {
			le_opt |= PTL_LE_ACK_DISABLE;
		} else if (!strcmp("SUCCESS_DISABLE", tok)) {
			le_opt |= PTL_LE_EVENT_SUCCESS_DISABLE;
		} else if (!strcmp("IS_ACCESSIBLE", tok)) {
			le_opt |= PTL_LE_IS_ACCESSIBLE;
		} else if (!strcmp("EVENT_LINK_DISABLE", tok)) {
			le_opt |= PTL_LE_EVENT_LINK_DISABLE;
		} else if (!strcmp("COMM_DISABLE", tok)) {
			le_opt |= PTL_LE_EVENT_COMM_DISABLE;
		} else if (!strcmp("FLOW_DISABLE", tok)) {
			le_opt |= PTL_LE_EVENT_FLOWCTRL_DISABLE;
		} else if (!strcmp("OVER_DISABLE", tok)) {
			le_opt |= PTL_LE_EVENT_OVER_DISABLE;
		} else if (!strcmp("UNLINK_DISABLE", tok)) {
			le_opt |= PTL_LE_EVENT_UNLINK_DISABLE;
		} else if (!strcmp("CT_COMM", tok)) {
			le_opt |= PTL_LE_EVENT_CT_COMM;
		} else if (!strcmp("CT_OVERFLOW", tok)) {
			le_opt |= PTL_LE_EVENT_CT_OVERFLOW;
		} else if (!strcmp("CT_BYTES", tok)) {
			le_opt |= PTL_LE_EVENT_CT_BYTES;
		} else if (!strcmp("INVALID", tok)) {
			le_opt = ~PTL_LE_APPEND_OPTIONS_MASK;
		} else {
			le_opt = strtol(tok, NULL, 0);
			break;
		}
		val = NULL;
	}

	free(orig_val);

	return le_opt;
}

unsigned int get_me_opt(char *orig_val)
{
	char *val;
	char *tok;
	char *save;

	unsigned int me_opt = 0;

	orig_val = strdup(orig_val);
	val = orig_val;

	while((tok = strtok_r(val, " \t", &save))) {
		if (!strcmp("IOVEC", tok)) {
			me_opt |= PTL_IOVEC;
		} else if (!strcmp("OP_PUT", tok)) {
			me_opt |= PTL_ME_OP_PUT;
		} else if (!strcmp("OP_GET", tok)) {
			me_opt |= PTL_ME_OP_GET;
		} else if (!strcmp("USE_ONCE", tok)) {
			me_opt |= PTL_ME_USE_ONCE;
		} else if (!strcmp("ACK_DISABLE", tok)) {
			me_opt |= PTL_ME_ACK_DISABLE;
		} else if (!strcmp("SUCCESS_DISABLE", tok)) {
			me_opt |= PTL_ME_EVENT_SUCCESS_DISABLE;
		} else if (!strcmp("IS_ACCESSIBLE", tok)) {
			me_opt |= PTL_ME_IS_ACCESSIBLE;
		} else if (!strcmp("EVENT_LINK_DISABLE", tok)) {
			me_opt |= PTL_ME_EVENT_LINK_DISABLE;
		} else if (!strcmp("COMM_DISABLE", tok)) {
			me_opt |= PTL_ME_EVENT_COMM_DISABLE;
		} else if (!strcmp("FLOW_DISABLE", tok)) {
			me_opt |= PTL_ME_EVENT_FLOWCTRL_DISABLE;
		} else if (!strcmp("OVER_DISABLE", tok)) {
			me_opt |= PTL_ME_EVENT_OVER_DISABLE;
		} else if (!strcmp("UNLINK_DISABLE", tok)) {
			me_opt |= PTL_ME_EVENT_UNLINK_DISABLE;
		} else if (!strcmp("CT_COMM", tok)) {
			me_opt |= PTL_ME_EVENT_CT_COMM;
		} else if (!strcmp("CT_OVERFLOW", tok)) {
			me_opt |= PTL_ME_EVENT_CT_OVERFLOW;
		} else if (!strcmp("CT_BYTES", tok)) {
			me_opt |= PTL_ME_EVENT_CT_BYTES;
		} else if (!strcmp("MANAGE_LOCAL", tok)) {
			me_opt |= PTL_ME_MANAGE_LOCAL;
		} else if (!strcmp("NO_TRUNCATE", tok)) {
			me_opt |= PTL_ME_NO_TRUNCATE;
		} else if (!strcmp("MAY_ALIGN", tok)) {
			me_opt |= PTL_ME_MAY_ALIGN;
		} else if (!strcmp("INVALID", tok)) {
			me_opt = ~PTL_ME_APPEND_OPTIONS_MASK;
		} else {
			me_opt = strtol(tok, NULL, 0);
			break;
		}
		val = NULL;
	}

	free(orig_val);

	return me_opt;
}

/*
 * enum.c - parse portals API enumerated types
 */

#include "ptl_test.h"

int get_ret(char *val)
{
	     if (!strcmp("OK", val))		return PTL_OK;
	else if (!strcmp("FAIL", val))		return PTL_FAIL;
	else if (!strcmp("ARG_INVALID", val))	return PTL_ARG_INVALID;
	else if (!strcmp("CT_NONE_REACHED", val)) return PTL_CT_NONE_REACHED;
	else if (!strcmp("EQ_DROPPED", val))	return PTL_EQ_DROPPED;
	else if (!strcmp("EQ_EMPTY", val))	return PTL_EQ_EMPTY;
	else if (!strcmp("IN_USE", val))	return PTL_IN_USE;
	else if (!strcmp("INTERRUPTED", val))	return PTL_INTERRUPTED;
	else if (!strcmp("LIST_TOO_LONG", val))	return PTL_LIST_TOO_LONG;
	else if (!strcmp("NO_INIT", val))	return PTL_NO_INIT;
	else if (!strcmp("NO_SPACE", val))	return PTL_NO_SPACE;
	else if (!strcmp("PID_IN_USE", val))	return PTL_PID_IN_USE;
	else if (!strcmp("PT_FULL", val))	return PTL_PT_FULL;
	else if (!strcmp("PT_EQ_NEEDED", val))	return PTL_PT_EQ_NEEDED;
	else if (!strcmp("PT_IN_USE", val))	return PTL_PT_IN_USE;
	else if (!strcmp("SIZE_INVALID", val))	return PTL_SIZE_INVALID;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

int get_atom_op(char *val)
{
	     if (!strcmp("MIN", val))		return PTL_MIN;
	else if (!strcmp("MAX", val))		return PTL_MAX;
	else if (!strcmp("SUM", val))		return PTL_SUM;
	else if (!strcmp("PROD", val))		return PTL_PROD;
	else if (!strcmp("LOR", val))		return PTL_LOR;
	else if (!strcmp("LAND", val))		return PTL_LAND;
	else if (!strcmp("BOR", val))		return PTL_BOR;
	else if (!strcmp("BAND", val))		return PTL_BAND;
	else if (!strcmp("LXOR", val))		return PTL_LXOR;
	else if (!strcmp("BXOR", val))		return PTL_BXOR;
	else if (!strcmp("SWAP", val))		return PTL_SWAP;
	else if (!strcmp("CSWAP", val))		return PTL_CSWAP;
	else if (!strcmp("CSWAP_NE", val))	return PTL_CSWAP_NE;
	else if (!strcmp("CSWAP_LE", val))	return PTL_CSWAP_LE;
	else if (!strcmp("CSWAP_LT", val))	return PTL_CSWAP_LT;
	else if (!strcmp("CSWAP_GE", val))	return PTL_CSWAP_GE;
	else if (!strcmp("CSWAP_GT", val))	return PTL_CSWAP_GT;
	else if (!strcmp("MSWAP", val))		return PTL_MSWAP;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

int get_atom_type(char *val)
{
	     if (!strcmp("CHAR", val))		return PTL_INT8_T;
	else if (!strcmp("UCHAR", val))		return PTL_UINT8_T;
	else if (!strcmp("SHORT", val))		return PTL_INT16_T;
	else if (!strcmp("USHORT", val))	return PTL_UINT16_T;
	else if (!strcmp("INT", val))		return PTL_INT32_T;
	else if (!strcmp("UINT", val))		return PTL_UINT32_T;
	else if (!strcmp("LONG", val))		return PTL_INT64_T;
	else if (!strcmp("ULONG", val))		return PTL_UINT64_T;
	else if (!strcmp("FLOAT", val))		return PTL_FLOAT;
	else if (!strcmp("COMPLEX", val))	return PTL_FLOAT_COMPLEX;
	else if (!strcmp("DOUBLE", val))	return PTL_DOUBLE;
	else if (!strcmp("DCOMPLEX", val))	return PTL_DOUBLE_COMPLEX;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

int get_list(char *val)
{
	     if (!strcmp("PRIORITY", val))	return PTL_PRIORITY_LIST;
	else if (!strcmp("OVERFLOW", val))	return PTL_OVERFLOW;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

int get_search_op(char *val)
{
	     if (!strcmp("ONLY", val))		return PTL_SEARCH_ONLY;
	else if (!strcmp("DELETE", val))	return PTL_SEARCH_DELETE;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

int get_ack_req(char *val)
{
	     if (!strcmp("ACK", val))		return PTL_ACK_REQ;
	else if (!strcmp("NO_ACK", val))	return PTL_NO_ACK_REQ;
	else if (!strcmp("CT_ACK", val))	return PTL_CT_ACK_REQ;
	else if (!strcmp("OC_ACK", val))	return PTL_OC_ACK_REQ;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

int get_event_type(char *val)
{
	     if (!strcmp("GET", val))		return PTL_EVENT_GET;
	else if (!strcmp("PUT", val))		return PTL_EVENT_PUT;
	else if (!strcmp("PUT_OVERFLOW", val))	return PTL_EVENT_PUT_OVERFLOW;
	else if (!strcmp("ATOMIC", val))	return PTL_EVENT_ATOMIC;
	else if (!strcmp("ATOMIC_OVERFLOW", val)) return PTL_EVENT_ATOMIC_OVERFLOW;
	else if (!strcmp("REPLY", val))		return PTL_EVENT_REPLY;
	else if (!strcmp("SEND", val))		return PTL_EVENT_SEND;
	else if (!strcmp("ACK", val))		return PTL_EVENT_ACK;
	else if (!strcmp("PT_DISABLED", val))	return PTL_EVENT_PT_DISABLED;
	else if (!strcmp("AUTO_UNLINK", val))	return PTL_EVENT_AUTO_UNLINK;
	else if (!strcmp("AUTO_FREE", val))	return PTL_EVENT_AUTO_FREE;
	else if (!strcmp("SEARCH", val))	return PTL_EVENT_SEARCH;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

int get_ni_fail(char *val)
{
	     if (!strcmp("OK", val))		return PTL_NI_OK;
	else if (!strcmp("UNDELIVERABLE", val))	return PTL_NI_UNDELIVERABLE;
	else if (!strcmp("FLOW_CTRL", val))	return PTL_NI_FLOW_CTRL;
	else if (!strcmp("DROPPED", val))	return PTL_NI_DROPPED;
	else if (!strcmp("PERM_VIOLATION", val))return PTL_NI_PERM_VIOLATION;
	else if (!strcmp("INVALID", val))	return 0xffffffff;
	else					return strtol(val, NULL, 0);
}

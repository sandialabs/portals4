/*
 * dict.c
 */

#include "ptl_test.h"

struct dict_entry *hash_table[1024];

struct dict_init {
	char		*name;
	int		type;
	int		token;
} dict_init[] = {
	{"set",				TYPE_NODE,		NODE_SET},
	{"check",			TYPE_NODE,		NODE_CHECK},
	{"if",				TYPE_NODE,		NODE_IF},
	{"else",			TYPE_NODE,		NODE_ELSE},
	{"test",			TYPE_NODE,		NODE_TEST},
	{"threads",			TYPE_NODE,		NODE_THREADS},
	{"subtest",			TYPE_NODE,		NODE_SUBTEST},
	{"desc",			TYPE_NODE,		NODE_DESC},
	{"comment",			TYPE_NODE,		NODE_COMMENT},
	{"repeat",			TYPE_NODE,		NODE_REPEAT},
	{"msleep",			TYPE_NODE,		NODE_MSLEEP},
	{"dump_objects",		TYPE_NODE,		NODE_DUMP_OBJECTS},
	{"barrier",			TYPE_NODE,		NODE_BARRIER},
	{"ompi_rt",			TYPE_NODE,		NODE_OMPI_RT},
	{"ptl",				TYPE_NODE,		NODE_PTL},
	{"ptl_init",			TYPE_NODE,		NODE_PTL_INIT},
	{"ptl_fini",			TYPE_NODE,		NODE_PTL_FINI},
	{"ptl_ni",			TYPE_NODE,		NODE_PTL_NI},
	{"ptl_ni_init",			TYPE_NODE,		NODE_PTL_NI_INIT},
	{"ptl_ni_fini",			TYPE_NODE,		NODE_PTL_NI_FINI},
	{"ptl_ni_status",		TYPE_NODE,		NODE_PTL_NI_STATUS},
	{"ptl_ni_handle",		TYPE_NODE,		NODE_PTL_NI_HANDLE},
	{"ptl_pt",			TYPE_NODE,		NODE_PTL_PT},
	{"ptl_pt_alloc",		TYPE_NODE,		NODE_PTL_PT_ALLOC},
	{"ptl_pt_free",			TYPE_NODE,		NODE_PTL_PT_FREE},
	{"ptl_pt_disable",		TYPE_NODE,		NODE_PTL_PT_DISABLE},
	{"ptl_pt_enable",		TYPE_NODE,		NODE_PTL_PT_ENABLE},
	{"ptl_get_uid",			TYPE_NODE,		NODE_PTL_GET_UID},
	{"ptl_get_id",			TYPE_NODE,		NODE_PTL_GET_ID},
	{"ptl_get_jid",			TYPE_NODE,		NODE_PTL_GET_JID},
	{"ptl_md",			TYPE_NODE,		NODE_PTL_MD},
	{"ptl_md_bind",			TYPE_NODE,		NODE_PTL_MD_BIND},
	{"ptl_md_release",		TYPE_NODE,		NODE_PTL_MD_RELEASE},
	{"ptl_le",			TYPE_NODE,		NODE_PTL_LE},
	{"ptl_le_append",		TYPE_NODE,		NODE_PTL_LE_APPEND},
	{"ptl_le_unlink",		TYPE_NODE,		NODE_PTL_LE_UNLINK},
	{"ptl_me",			TYPE_NODE,		NODE_PTL_ME},
	{"ptl_me_append",		TYPE_NODE,		NODE_PTL_ME_APPEND},
	{"ptl_me_unlink",		TYPE_NODE,		NODE_PTL_ME_UNLINK},
	{"ptl_eq",			TYPE_NODE,		NODE_PTL_EQ},
	{"ptl_eq_alloc",		TYPE_NODE,		NODE_PTL_EQ_ALLOC},
	{"ptl_eq_free",			TYPE_NODE,		NODE_PTL_EQ_FREE},
	{"ptl_eq_get",			TYPE_NODE,		NODE_PTL_EQ_GET},
	{"ptl_eq_wait",			TYPE_NODE,		NODE_PTL_EQ_WAIT},
	{"ptl_eq_poll",			TYPE_NODE,		NODE_PTL_EQ_POLL},
	{"ptl_ct",			TYPE_NODE,		NODE_PTL_CT},
	{"ptl_ct_alloc",		TYPE_NODE,		NODE_PTL_CT_ALLOC},
	{"ptl_ct_free",			TYPE_NODE,		NODE_PTL_CT_FREE},
	{"ptl_ct_get",			TYPE_NODE,		NODE_PTL_CT_GET},
	{"ptl_ct_wait",			TYPE_NODE,		NODE_PTL_CT_WAIT},
	{"ptl_ct_poll",			TYPE_NODE,		NODE_PTL_CT_POLL},
	{"ptl_ct_set",			TYPE_NODE,		NODE_PTL_CT_SET},
	{"ptl_ct_inc",			TYPE_NODE,		NODE_PTL_CT_INC},
	{"ptl_put",			TYPE_NODE,		NODE_PTL_PUT},
	{"ptl_get",			TYPE_NODE,		NODE_PTL_GET},
	{"ptl_atomic",			TYPE_NODE,		NODE_PTL_ATOMIC},
	{"ptl_fetch",			TYPE_NODE,		NODE_PTL_FETCH},
	{"ptl_swap",			TYPE_NODE,		NODE_PTL_SWAP},
	{"ptl_trig_put",		TYPE_NODE,		NODE_PTL_TRIG_PUT},
	{"ptl_trig_get",		TYPE_NODE,		NODE_PTL_TRIG_GET},
	{"ptl_trig_atomic",		TYPE_NODE,		NODE_PTL_TRIG_ATOMIC},
	{"ptl_trig_fetch",		TYPE_NODE,		NODE_PTL_TRIG_FETCH},
	{"ptl_trig_swap",		TYPE_NODE,		NODE_PTL_TRIG_SWAP},
	{"ptl_trig_ct_inc",		TYPE_NODE,		NODE_PTL_TRIG_CT_INC},
	{"ptl_trig_ct_set",		TYPE_NODE,		NODE_PTL_TRIG_CT_SET},
	{"ptl_handle_is_equal",		TYPE_NODE,		NODE_PTL_HANDLE_IS_EQUAL},
	{"ptl_set_jid",			TYPE_NODE,		NODE_PTL_SET_JID},

	{"ret",				TYPE_ATTR,		ATTR_RET},
	{"ptr",				TYPE_ATTR,		ATTR_PTR},
	{"count",			TYPE_ATTR,		ATTR_COUNT},
	{"thread_id",			TYPE_ATTR,		ATTR_THREAD_ID},

	{"iface",			TYPE_ATTR,		ATTR_IFACE},
	{"ni_opt",			TYPE_ATTR,		ATTR_NI_OPT},
	{"pid",				TYPE_ATTR,		ATTR_PID},
	{"uid",				TYPE_ATTR,		ATTR_UID},
	{"jid",				TYPE_ATTR,		ATTR_JID},
	{"desired_max_entries",		TYPE_ATTR,		ATTR_DESIRED_MAX_ENTRIES},
	{"desired_max_over",		TYPE_ATTR,		ATTR_DESIRED_MAX_OVER},
	{"desired_max_mds",		TYPE_ATTR,		ATTR_DESIRED_MAX_MDS},
	{"desired_max_eqs",		TYPE_ATTR,		ATTR_DESIRED_MAX_EQS},
	{"desired_max_cts",		TYPE_ATTR,		ATTR_DESIRED_MAX_CTS},
	{"desired_max_pt_index",	TYPE_ATTR,		ATTR_DESIRED_MAX_PT_INDEX},
	{"desired_max_iovecs",		TYPE_ATTR,		ATTR_DESIRED_MAX_IOVECS},
	{"desired_max_list_size",	TYPE_ATTR,		ATTR_DESIRED_MAX_LIST_SIZE},
	{"desired_max_msg_size",	TYPE_ATTR,		ATTR_DESIRED_MAX_MSG_SIZE},
	{"desired_max_atomic_size",	TYPE_ATTR,		ATTR_DESIRED_MAX_ATOMIC_SIZE},
	{"actual_max_entries",		TYPE_ATTR,		ATTR_ACTUAL_MAX_ENTRIES},
	{"actual_max_over",		TYPE_ATTR,		ATTR_ACTUAL_MAX_OVER},
	{"actual_max_mds",		TYPE_ATTR,		ATTR_ACTUAL_MAX_MDS},
	{"actual_max_eqs",		TYPE_ATTR,		ATTR_ACTUAL_MAX_EQS},
	{"actual_max_cts",		TYPE_ATTR,		ATTR_ACTUAL_MAX_CTS},
	{"actual_max_pt_index",		TYPE_ATTR,		ATTR_ACTUAL_MAX_PT_INDEX},
	{"actual_max_iovecs",		TYPE_ATTR,		ATTR_ACTUAL_MAX_IOVECS},
	{"actual_max_list_size",	TYPE_ATTR,		ATTR_ACTUAL_MAX_LIST_SIZE},
	{"actual_max_msg_size",		TYPE_ATTR,		ATTR_ACTUAL_MAX_MSG_SIZE},
	{"actual_max_atomic_size",	TYPE_ATTR,		ATTR_ACTUAL_MAX_ATOMIC_SIZE},
	{"ni_handle",			TYPE_ATTR,		ATTR_NI_HANDLE},
	{"reg",				TYPE_ATTR,		ATTR_SR_INDEX},
	{"status",			TYPE_ATTR,		ATTR_SR_VALUE},
	{"handle",			TYPE_ATTR,		ATTR_HANDLE},

	{"pt_opt",			TYPE_ATTR,		ATTR_PT_OPT},
	{"pt_index",			TYPE_ATTR,		ATTR_PT_INDEX},
	{"iov_base",			TYPE_ATTR,		ATTR_IOV_BASE},
	{"md_start",			TYPE_ATTR,		ATTR_MD_START},
	{"md_length",			TYPE_ATTR,		ATTR_MD_LENGTH},
	{"md_opt",			TYPE_ATTR,		ATTR_MD_OPT},
	{"md_handle",			TYPE_ATTR,		ATTR_MD_HANDLE},
	{"md_data",			TYPE_ATTR,		ATTR_MD_DATA},
	{"list",			TYPE_ATTR,		ATTR_LIST},

	{"le_start",			TYPE_ATTR,		ATTR_LE_START},
	{"le_length",			TYPE_ATTR,		ATTR_LE_LENGTH},
	{"le_opt",			TYPE_ATTR,		ATTR_LE_OPT},
	{"le_data",			TYPE_ATTR,		ATTR_LE_DATA},

	{"me_start",			TYPE_ATTR,		ATTR_ME_START},
	{"me_length",			TYPE_ATTR,		ATTR_ME_LENGTH},
	{"me_match",			TYPE_ATTR,		ATTR_ME_MATCH},
	{"me_ignore",			TYPE_ATTR,		ATTR_ME_IGNORE},
	{"me_opt",			TYPE_ATTR,		ATTR_ME_OPT},
	{"me_min_free",			TYPE_ATTR,		ATTR_ME_MIN_FREE},
	{"me_data",			TYPE_ATTR,		ATTR_ME_DATA},

	{"time",			TYPE_ATTR,		ATTR_TIME},
	{"which",			TYPE_ATTR,		ATTR_WHICH},
	{"which_ptr",			TYPE_ATTR,		ATTR_WHICH_PTR},
	{"eq_count",			TYPE_ATTR,		ATTR_EQ_COUNT},
	{"eq_handle",			TYPE_ATTR,		ATTR_EQ_HANDLE},

	{"event_type",			TYPE_ATTR,		ATTR_EVENT_TYPE},
	{"event_nid",			TYPE_ATTR,		ATTR_EVENT_NID},
	{"event_pid",			TYPE_ATTR,		ATTR_EVENT_PID},
	{"event_rank",			TYPE_ATTR,		ATTR_EVENT_RANK},
	{"event_pt_index",		TYPE_ATTR,		ATTR_EVENT_PT_INDEX},
	{"event_uid",			TYPE_ATTR,		ATTR_EVENT_UID},
	{"event_jid",			TYPE_ATTR,		ATTR_EVENT_JID},
	{"event_match",			TYPE_ATTR,		ATTR_EVENT_MATCH},
	{"event_rlength",		TYPE_ATTR,		ATTR_EVENT_RLENGTH},
	{"event_mlength",		TYPE_ATTR,		ATTR_EVENT_MLENGTH},
	{"event_offset",		TYPE_ATTR,		ATTR_EVENT_OFFSET},
	{"event_start",			TYPE_ATTR,		ATTR_EVENT_START},
	{"event_user_ptr",		TYPE_ATTR,		ATTR_EVENT_USER_PTR},
	{"event_hdr_data",		TYPE_ATTR,		ATTR_EVENT_HDR_DATA},
	{"event_ni_fail",		TYPE_ATTR,		ATTR_EVENT_NI_FAIL},
	{"event_atom_op",		TYPE_ATTR,		ATTR_EVENT_ATOM_OP},
	{"event_atom_type",		TYPE_ATTR,		ATTR_EVENT_ATOM_TYPE},

	{"ct_handle",			TYPE_ATTR,		ATTR_CT_HANDLE},
	{"ct_event_success",		TYPE_ATTR,		ATTR_CT_EVENT_SUCCESS},
	{"ct_event_failure",		TYPE_ATTR,		ATTR_CT_EVENT_FAILURE},
	{"ct_test",			TYPE_ATTR,		ATTR_CT_TEST},

	{"length",			TYPE_ATTR,		ATTR_LENGTH},
	{"offset",			TYPE_ATTR,		ATTR_OFFSET},
	{"operand",			TYPE_ATTR,		ATTR_OPERAND},
	{"type",			TYPE_ATTR,		ATTR_TYPE},
	{"loc_offset",			TYPE_ATTR,		ATTR_LOC_OFFSET},
	{"loc_get_offset",		TYPE_ATTR,		ATTR_LOC_GET_OFFSET},
	{"loc_put_offset",		TYPE_ATTR,		ATTR_LOC_PUT_OFFSET},
	{"rem_offset",			TYPE_ATTR,		ATTR_REM_OFFSET},
	{"atom_op",			TYPE_ATTR,		ATTR_ATOM_OP},
	{"atom_type",			TYPE_ATTR,		ATTR_ATOM_TYPE},
	{"match",			TYPE_ATTR,		ATTR_MATCH},
	{"get_md_handle",		TYPE_ATTR,		ATTR_GET_MD_HANDLE},
	{"put_md_handle",		TYPE_ATTR,		ATTR_PUT_MD_HANDLE},
	{"user_ptr",			TYPE_ATTR,		ATTR_USER_PTR},
	{"ack_req",			TYPE_ATTR,		ATTR_ACK_REQ},
	{"handle1",			TYPE_ATTR,		ATTR_HANDLE1},
	{"handle2",			TYPE_ATTR,		ATTR_HANDLE2},
	{"threshold",			TYPE_ATTR,		ATTR_THRESHOLD},

	{"rank",			TYPE_ATTR,		ATTR_RANK},
	{"target_id",			TYPE_ATTR,		ATTR_TARGET_ID},

	/* used to mark end of list keep last */
	{NULL,				0,			0},
};

/* loosely based on http://sites.google.com/site/murmurhash/MurmurHash2.cpp */
static unsigned int hash(const char *data)
{
	const unsigned int m = 0x5bd1e995;
	unsigned int len = strlen(data);
	unsigned int h = len;
	unsigned int k;

	while(len >= 4) {
		k = *(unsigned int *)data;

		k *= m; 
		k ^= k >> 24; 
		k *= m; 

		h *= m; 
		h ^= k;

		data += 4;
		len -= 4;
	}

	switch(len) {
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
		h *= m;
	};

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
} 

struct dict_entry *lookup(const char *name)
{
	int h = hash(name) & 1023;
	struct dict_entry *e;

	for (e = hash_table[h]; e; e = e->next) {
		if (!strcmp(name, e->name))
			return e;
	}

	return NULL;
}

struct dict_entry *insert(const char *name)
{
	int h = hash(name) & 1023;
	struct dict_entry *e;

	for (e = hash_table[h]; e; e = e->next) {
		if (!strcmp(name, e->name))
			return e;
	}

	e = calloc(1, sizeof(*e));
	e->name = strdup(name);
	e->next = hash_table[h];
	hash_table[h] = e;

	return e;
}

void init_dict(void)
{
	struct dict_init *init;
	struct dict_entry *e;

	for (init = dict_init; init->name; init++) {
		e = insert(init->name);
		e->type = init->type;
		e->token = init->token;
	}
}

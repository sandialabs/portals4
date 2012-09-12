/* run test */

#include "ptl_test.h"

static ptl_handle_any_t get_handle(struct node_info *info, char *val)
{
	int n;
	char obj[8];
	ptl_handle_any_t handle;

	if (!strcmp("INVALID", val))
		handle = PTL_INVALID_HANDLE;
	else if (!strcmp("CT_NONE", val))
		handle = PTL_CT_NONE;
	else if (!strcmp("EQ_NONE", val))
		handle = PTL_EQ_NONE;
	else if (sscanf(val, "%2s[%d]", obj, &n) == 2) {
		if (n < 0 || n >= STACK_SIZE) {
			printf("invalid index n = %d\n", n);
			handle =  0xffffffffU;
		}
		else if (!strcmp(obj, "md"))
			handle =  info->md_stack[n];
		else if (!strcmp(obj, "me"))
			handle =  info->me_stack[n];
		else if (!strcmp(obj, "le"))
			handle =  info->le_stack[n];
		else if (!strcmp(obj, "ni"))
			handle =  info->ni_stack[n];
		else if (!strcmp(obj, "pt"))
			handle =  info->pt_stack[n];
		else if (!strcmp(obj, "eq"))
			handle =  info->eq_stack[n];
		else if (!strcmp(obj, "ct"))
			handle =  info->ct_stack[n];
		else {
			printf("invalid object %s\n", obj);
			handle =  0xffffffffU;
		}
	} else {
		char *endptr;
		handle = (ptl_handle_any_t)strtoull(val, &endptr, 0);
		if (*endptr)
			printf("Invalid handle value: %s\n", val);
	}

	return handle;
}

static long get_number(struct node_info *info, char *orig_val)
{
	char *val;
	long num;
	char *tok[4];
	char *save = NULL;
	int i;

	orig_val = strdup(orig_val);
	val = orig_val;

	if (info && val[0] == '$') {
		val++;
		i = 0;
		while((tok[i++] = strtok_r(val, ".", &save)))
			val = NULL;

		if (tok[0]) {
			if (!strcmp("count", tok[0])) {
				num = info->count;
			}

			else if (!strcmp("thread_id", tok[0])) {
				num = info->thread_id;
			}

			else if (!strcmp("actual", tok[0])) {
				if (tok[1] && !strcmp("max_pt_index", tok[1])) {
					num = info->actual.max_pt_index;
				} else {
					num = 0;
				}
			} else {
				num = 0;
			}
		} else {
			num = 0;
		}
	} else {
		num = strtol(val, NULL, 0);
	}

	free(orig_val);
	return num;
}

static datatype_t get_datatype(ptl_datatype_t type, char *val)
{
	datatype_t num;
	float f[2];
	double d[2];
	long double ld[2];

	num.u64 = 0;

	switch (type) {
	case PTL_INT8_T:
		num.s8 = strtol(val, NULL, 0);
		break;
	case PTL_UINT8_T:
		num.u8 = strtoul(val, NULL, 0);
		break;
	case PTL_INT16_T:
		num.s16 = strtol(val, NULL, 0);
		break;
	case PTL_UINT16_T:
		num.u16 = strtoul(val, NULL, 0);
		break;
	case PTL_INT32_T:
		num.s32 = strtol(val, NULL, 0);
		break;
	case PTL_UINT32_T:
		num.u32 = strtoul(val, NULL, 0);
		break;
	case PTL_INT64_T:
		num.s64 = strtoll(val, NULL, 0);
		break;
	case PTL_UINT64_T:
		num.u64 = strtoull(val, NULL, 0);
		break;
	case PTL_FLOAT:
		num.f = strtof(val, NULL);
		break;
	case PTL_FLOAT_COMPLEX:
		sscanf(val, "(%f, %f)", &f[0], &f[1]);
		num.fc = f[0] + f[1] * _Complex_I;
		break;
	case PTL_DOUBLE:
		num.d = strtod(val, NULL);
		break;
	case PTL_DOUBLE_COMPLEX:
		sscanf(val, "(%lf, %lf)", &d[0], &d[1]);
		num.dc = d[0] + d[1] * _Complex_I;
		break;
	case PTL_LONG_DOUBLE:
		num.ld = strtold(val, NULL);
		break;
	case PTL_LONG_DOUBLE_COMPLEX:
		sscanf(val, "(%Lf, %Lf)", &ld[0], &ld[1]);
		num.ldc = ld[0] + ld[1] * _Complex_I;
		break;
	default:
		printf("invalid type in get_datatype\n");
		break;
	}

	return num;
}

static int get_uid(struct node_info *info, char *val)
{
	if (!strcmp("ANY", val)) return PTL_UID_ANY;
	else return get_number(info, val);

}

static ptl_process_t get_target_id(struct node_info *info, char *val)
{
	ptl_process_t id;
	char *dotpos;

	memset(&id, 0, sizeof(id));

	if (!strcmp(val, "SELF")) {
		if (PtlGetId(info->ni_handle, &id) != PTL_OK) {
			printf("PtlGetId failed\n");
		}
	} else if ((dotpos = strchr(val, ':'))) {
		/* Physical ID. */
		id.phys.nid = get_number(info, val);
		dotpos ++;
		id.phys.pid = get_number(info, dotpos);
	} else {
		/* Logical ID. */
		id.rank = get_number(info, val);
	}

	return id;
}

static int get_index(struct node_info *info, char *val)
{
	int index;

	if (!strcmp("MIN", val))
		index = 0;
	else if (!strcmp("MAX", val))
		index = info->actual.max_pt_index;
	else if (!strcmp("BIG", val))
		index = info->actual.max_pt_index + 1;
	else if (!strcmp("ANY", val))
		index = PTL_PT_ANY;
	else if (!strcmp("INVALID", val))
		index = 0x7fffffff;
	else
		index = strtol(val, NULL, 0);

	return index;
}

static void *get_ptr(char *val)
{
	void *ptr;

	if (!strcmp("NULL", val))
		ptr = NULL;
	else if (!strcmp("BAD", val))
		ptr = (void *)0x0123;
	else
		ptr = (void *)(uintptr_t)strtoll(val, NULL, 0);

	return ptr;
}

/*
 * push_info
 *	this routine is called in preparation for executing a new operation
 *	it creates a new parameter set that is copied from the previous one
 *	with some operation specific configuration and some defaults
 */
static struct node_info *push_info(struct node_info *head, int tok)
{
	int i;
	struct node_info *info;

	info = calloc(1, sizeof *info);
	if (!info) {
		printf("unable to allocate node_info\n");
		return NULL;
	}

	*info = *head;
	info->buf_alloc = 0;

	info->next = head;
	head->prev = info;

	/* defaults */
	info->count = 1;
	info->ret = PTL_OK;
	info->err = PTL_OK;
	info->type = PTL_UINT8_T;

	/* If token is MD/LE/ME then allocate current largest buffer */
	switch(tok) {
	case NODE_PTL_MD:
	case NODE_PTL_MD_BIND:
	case NODE_PTL_ME:
	case NODE_PTL_ME_APPEND:
	case NODE_PTL_LE:
	case NODE_PTL_LE_APPEND:
		info->buf = calloc(1, info->actual.max_msg_size);
		if (!info->buf) {
			printf("unable to allocate md/me/le buffer\n");
			free(info);
			return NULL;
		}
		for (i = 0; i < IOV_SIZE; i++) {
			info->iov[i].iov_base	= info->buf + info->iovec_length*i;
			info->iov[i].iov_len	= info->iovec_length;
		}
		info->buf_alloc = 1;
		break;
	default:
		for (i = 0; i < IOV_SIZE; i++) {
			info->iov[i].iov_base	= NULL;
			info->iov[i].iov_len	= 0;
		}
		break;
	}

	info->md.start			= info->buf;
	info->md.length			= info->actual.max_msg_size;
	info->md.options		= 0;

	info->le.start			= info->buf;
	info->le.length			= info->actual.max_msg_size;
	info->le.options		= 0;

	info->me.start			= info->buf;
	info->me.length			= info->actual.max_msg_size;
	info->me.options		= 0;
	info->me.min_free		= 0;

	if (info->ni_opt & PTL_NI_PHYSICAL) {
		info->me.match_id.phys.pid  = PTL_PID_ANY;
		info->me.match_id.phys.nid  = PTL_NID_ANY;
	} else
		info->me.match_id.rank  = PTL_RANK_ANY;

	switch(tok) {
	case NODE_PTL_NI:
	case NODE_PTL_NI_INIT:
		info->ptr = &info->ni_handle;
		info->desired_ptr = &info->desired;
		info->actual_ptr = &info->actual;
		info->ni_opt = PTL_NI_MATCHING | PTL_NI_PHYSICAL;
		break;
	case NODE_PTL_NI_STATUS:
		info->ptr = &info->status;
		break;
	case NODE_PTL_NI_HANDLE:
		info->ptr = &info->ni_handle;
		break;
	case NODE_PTL_GET_UID:
		info->ptr = &info->uid;
		break;
	case NODE_PTL_GET_ID:
		info->ptr = &info->id;
		break;
	case NODE_PTL_PT:
	case NODE_PTL_PT_ALLOC:
		info->ptr = &info->pt_index;
		info->pt_index = 0;
		info->pt_opt = 0;
		break;
	case NODE_PTL_EQ:
	case NODE_PTL_EQ_ALLOC:
		info->ptr = &info->eq_handle;
		break;
	case NODE_PTL_EQ_GET:
		info->ptr = &info->eq_event;
		break;
	case NODE_PTL_EQ_WAIT:
		info->ptr = &info->eq_event;
		break;
	case NODE_PTL_EQ_POLL:
		info->ptr = &info->eq_event;
		info->which_ptr = &info->which;
		info->which = -1;
		info->eq_event.type = -1;
		break;
	case NODE_PTL_CT:
	case NODE_PTL_CT_ALLOC:
		info->ptr = &info->ct_handle;
		break;
	case NODE_PTL_CT_GET:
		info->ptr = &info->ct_event;
		break;
	case NODE_PTL_CT_WAIT:
		info->ptr = &info->ct_event;
		break;
	case NODE_PTL_CT_POLL:
		info->ptr = &info->ct_event;
		info->which_ptr = &info->which;
		break;
	case NODE_PTL_MD:
	case NODE_PTL_MD_BIND:
		info->ptr = &info->md_handle;
		info->md.options = 0;
		break;
	case NODE_PTL_LE:
	case NODE_PTL_LE_APPEND:
		info->ptr = &info->le_handle;
		info->le.options = 0;
		break;
	case NODE_PTL_ME:
	case NODE_PTL_ME_APPEND:
		info->ptr = &info->me_handle;
		info->me.options = 0;
		break;
	case NODE_PTL_FETCH:
	case NODE_PTL_TRIG_FETCH:
		if (info->next_md >= 2) {
			info->get_md_handle = info->md_stack[info->next_md - 1];
			info->put_md_handle = info->md_stack[info->next_md - 2];
		} else {
			info->get_md_handle = get_handle(info, "INVALID");
			info->put_md_handle = get_handle(info, "INVALID");
		}
		info->ptr = &info->operand;
		break;
	case NODE_PTL_SWAP:
	case NODE_PTL_TRIG_SWAP:
		if (info->next_md >= 2) {
			info->get_md_handle = info->md_stack[info->next_md - 1];
			info->put_md_handle = info->md_stack[info->next_md - 2];
		} else {
			info->get_md_handle = get_handle(info, "INVALID");
			info->put_md_handle = get_handle(info, "INVALID");
		}
		info->ptr = &info->operand;
		info->atom_op = PTL_SWAP;
		break;
	}

	return info;
}

static struct node_info *pop_node(struct node_info *info)
{
	struct node_info *head;

	head = info->next;
	head->prev = NULL;

	if (info->buf_alloc) {
		free(info->buf);
	}
	free(info);

	return head;
}

/*
 * set_data
 *	initialize a buffer with repeated numeric values of
 *	a given type
 */
static int set_data(datatype_t val, void *data, int type, int length)
{
	uint8_t *p_u8;
	int8_t *p_8;
	uint16_t *p_u16;
	int16_t *p_16;
	uint32_t *p_u32;
	int32_t *p_32;
	uint64_t *p_u64;
	int64_t *p_64;
	float *p_f;
	float complex *p_fc;
	double *p_d;
	double complex *p_dc;
	long double *p_ld;
	long double complex *p_ldc;
	int i;

	switch(type) {
	case PTL_INT8_T:
		p_8 = data;
		for (i = 0; i < length; i++, p_8++)
			*p_8 = val.s8;
		break;
	case PTL_UINT8_T:
		p_u8 = data;
		for (i = 0; i < length; i++, p_u8++)
			*p_u8 = val.u8;
		break;
	case PTL_INT16_T:
		p_16 = data;
		for (i = 0; i < length/2; i++, p_16++)
			*p_16 = val.s16;
		break;
	case PTL_UINT16_T:
		p_u16 = data;
		for (i = 0; i < length/2; i++, p_u16++)
			*p_u16 = val.u16;
		break;
	case PTL_INT32_T:
		p_32 = data;
		for (i = 0; i < length/4; i++, p_32++)
			*p_32 = val.s32;
		break;
	case PTL_UINT32_T:
		p_u32 = data;
		for (i = 0; i < length/4; i++, p_u32++)
			*p_u32 = val.u32;
		break;
	case PTL_INT64_T:
		p_64 = data;
		for (i = 0; i < length/8; i++, p_64++)
			*p_64 = val.s64;
		break;
	case PTL_UINT64_T:
		p_u64 = data;
		for (i = 0; i < length/8; i++, p_u64++)
			*p_u64 = val.u64;
		break;
	case PTL_FLOAT:
		p_f = data;
		for (i = 0; i < length/4; i++, p_f++)
			*p_f = val.f;
		break;
	case PTL_FLOAT_COMPLEX:
		p_fc = data;
		for (i = 0; i < length/8; i++, p_fc ++)
			*p_fc = val.fc;
		break;
	case PTL_DOUBLE:
		p_d = data;
		for (i = 0; i < length/8; i++, p_d++)
			*p_d = val.d;
		break;
	case PTL_DOUBLE_COMPLEX:
		p_dc = data;
		for (i = 0; i < length/16; i++, p_dc ++)
			*p_dc = val.dc;
		break;
	case PTL_LONG_DOUBLE:
		p_ld = data;
		for (i = 0; i < length/sizeof(*p_ld); i++, p_ld++)
			*p_ld = val.ld;
		break;
	case PTL_LONG_DOUBLE_COMPLEX:
		p_ldc = data;
		for (i = 0; i < length/sizeof(*p_ldc); i++, p_ldc ++)
			*p_ldc = val.ldc;
		break;
	}

	return 0;
}

static int get_attr(struct node_info *info, xmlNode *node)
{
	xmlAttr *attr;
	char *val;
	struct dict_entry *e;
	int set_md_start = 0;
	int set_md_len = 0;
	int set_md_data = 0;
	int set_le_start = 0;
	int set_le_len = 0;
	int set_le_data = 0;
	int set_me_start = 0;
	int set_me_len = 0;
	int set_me_data = 0;
	char *data_val = data_val;

	for (attr = node->properties; attr; attr = attr->next) {
		val = (char *)attr->children->content;
		e = lookup((char *)attr->name);
		if (!e) {
			printf("invalid attr: %s\n", attr->name);
			return 1;
		}

		switch (e->token) {
		case ATTR_RET:
			info->ret = get_ret(val);
			break;
		case ATTR_PTR:
			info->ptr = get_ptr(val);
			break;
		case ATTR_COUNT:
			info->count = get_number(info, val);
			break;

		/* ni */
		case ATTR_IFACE:
			info->iface = strtol(val, NULL, 0);
			break;
		case ATTR_NI_OPT:
			info->ni_opt = get_ni_opt(val);
			break;
		case ATTR_PID:
			info->pid = get_number(info, val);
			break;
		case ATTR_UID:
			info->uid = get_uid(info, val);
			break;
		case ATTR_DESIRED_MAX_ENTRIES:
			info->desired.max_entries = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_MDS:
			info->desired.max_mds = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_EQS:
			info->desired.max_eqs = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_CTS:
			info->desired.max_cts = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_PT_INDEX:
			info->desired.max_pt_index = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_IOVECS:
			info->desired.max_iovecs = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_LIST_SIZE:
			info->desired.max_list_size = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_MSG_SIZE:
			info->desired.max_msg_size = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_ATOMIC_SIZE:
			info->desired.max_atomic_size = get_number(info, val);
			break;
		case ATTR_DESIRED_MAX_VOLATILE_SIZE:
			info->desired.max_volatile_size = get_number(info, val);
			break;
		case ATTR_DESIRED_FEATURES:
			info->desired.features = get_ni_features(val);
			break;
		case ATTR_NI_HANDLE:
			info->ni_handle = get_handle(info, val);
			break;
		case ATTR_SR_INDEX:
			info->reg = get_number(info, val);
			break;
		case ATTR_SR_VALUE:
			info->status = get_number(info, val);
			break;
		case ATTR_HANDLE:
			info->handle = get_handle(info, val);
			break;

		/* pt */
		case ATTR_PT_OPT:
			info->pt_opt = get_pt_opt(val);
			break;
		case ATTR_PT_INDEX:
			info->pt_index = get_index(info, val);
			break;

		case ATTR_LIST:
			info->list = get_list(val);
			break;

		case ATTR_IOV_BASE:
			info->iov[0].iov_base = get_ptr(val);
			break;

		case ATTR_IOVEC_LEN:
			info->iovec_length = get_number(info, val);
			{
				int i;
				for (i = 0; i < IOV_SIZE; i++) {
					info->iov[i].iov_base	= info->buf + info->iovec_length*i;
					info->iov[i].iov_len	= info->iovec_length;
				}
			}
			break;

		/* md */
		case ATTR_MD_START:
			info->md.start = get_ptr(val);
			set_md_start++;
			break;
		case ATTR_MD_LENGTH:
			info->md.length = get_number(info, val);
			set_md_len++;
			break;
		case ATTR_MD_OPT:
			info->md.options = get_md_opt(val);
			break;
		case ATTR_MD_HANDLE:
			info->md_handle = get_handle(info, val);
			break;
		case ATTR_MD_DATA:
			data_val = val;
			set_md_data++;
			break;

		/* le */
		case ATTR_LE_START:
			info->le.start = get_ptr(val);
			set_le_start++;
			break;
		case ATTR_LE_LENGTH:
			info->le.length = get_number(info, val);
			set_le_len++;
			break;
		case ATTR_LE_OPT:
			info->le.options = get_le_opt(val);
			break;
		case ATTR_LE_DATA:
			data_val = val;
			set_le_data++;
			break;
		case ATTR_SEARCH_OP:
			info->search_op = get_search_op(val);
			break;

		/* me */
		case ATTR_ME_START:
			info->me.start = get_ptr(val);
			set_me_start++;
			break;
		case ATTR_ME_LENGTH:
			info->me.length = get_number(info, val);
			set_me_len++;
			break;
		case ATTR_ME_MATCH:
			info->me.match_bits = get_number(info, val);
			break;
		case ATTR_ME_IGNORE:
			info->me.ignore_bits = get_number(info, val);
			break;
		case ATTR_ME_OPT:
			info->me.options = get_me_opt(val);
			break;
		case ATTR_ME_MIN_FREE:
			info->me.min_free = get_number(info, val);
			break;
		case ATTR_ME_DATA:
			data_val = val;
			set_me_data++;
			break;

		case ATTR_TIME:
			info->timeout = get_number(info, val);
			break;
		case ATTR_WHICH_PTR:
			info->which_ptr = get_ptr(val);
			break;

		/* eq */
		case ATTR_EQ_COUNT:
			info->eq_count = get_number(info, val);
			break;
		case ATTR_EQ_HANDLE:
			info->eq_handle = get_handle(info, val);
			break;

		/* ct */
		case ATTR_CT_HANDLE:
			info->ct_handle = get_handle(info, val);
			break;
		case ATTR_CT_EVENT_SUCCESS:
			info->ct_event.success = get_number(info, val);
			break;
		case ATTR_CT_EVENT_FAILURE:
			info->ct_event.failure = get_number(info, val);
			break;
		case ATTR_CT_TEST:
			info->ct_test = get_number(info, val);
			break;

		case ATTR_ATOM_OP:
			info->atom_op = get_atom_op(val);
			break;
		case ATTR_ATOM_TYPE:
			info->type = get_atom_type(val);
			break;
		case ATTR_GET_MD_HANDLE:
			info->get_md_handle = get_handle(info, val);
			break;
		case ATTR_PUT_MD_HANDLE:
			info->put_md_handle = get_handle(info, val);
			break;
		case ATTR_USER_PTR:
			info->user_ptr = get_ptr(val);
			break;
		case ATTR_TYPE:
			info->type = get_atom_type(val);
			break;
		case ATTR_LENGTH:
			info->length = get_number(info, val);
			break;
		case ATTR_OPERAND:
			info->operand = get_datatype(info->type, val);
			break;
		case ATTR_MATCH:
			info->match = get_number(info, val);
			break;
		case ATTR_LOC_OFFSET:
			info->loc_offset = get_number(info, val);
			break;
		case ATTR_LOC_GET_OFFSET:
			info->loc_get_offset = get_number(info, val);
			break;
		case ATTR_LOC_PUT_OFFSET:
			info->loc_put_offset = get_number(info, val);
			break;
		case ATTR_REM_OFFSET:
			info->rem_offset = get_number(info, val);
			break;
		case ATTR_ACK_REQ:
			info->ack_req = get_ack_req(val);
			break;
		case ATTR_THRESHOLD:
			info->threshold = get_number(info, val);
			break;
		case ATTR_HANDLE1:
			info->handle1 = get_handle(info, val);
			break;
		case ATTR_HANDLE2:
			info->handle2 = get_handle(info, val);
			break;
		case ATTR_EVENT_TYPE:
			info->eq_event.type = get_event_type(val);
			break;
		case ATTR_TARGET_ID:
			info->target_id = get_target_id(info, val);
			break;
		case ATTR_GET_MAP_SIZE:
			info->get_map_size = get_number(info, val);
			break;
		}
	}

	if (info->md.options & PTL_IOVEC) {
		if (!set_md_start)
			info->md.start = (void *)&info->iov[0];
		if (!set_md_len)
			info->md.length = IOV_SIZE;
	}

	if (info->le.options & PTL_IOVEC) {
		if (!set_le_start)
			info->le.start = (void *)&info->iov[0];
		if (!set_le_len)
			info->le.length = IOV_SIZE;
	}

	if (info->me.options & PTL_IOVEC) {
		if (!set_me_start)
			info->me.start = (void *)&info->iov[0];
		if (!set_me_len)
			info->me.length = IOV_SIZE;
	}

	if (set_md_data) {
		info->md_data = get_datatype(info->type, data_val);
		set_data(info->md_data, info->buf, info->type,
			info->actual.max_msg_size);
	}

	if (set_le_data) {
		info->le_data = get_datatype(info->type, data_val);
		set_data(info->le_data, info->buf, info->type,
			info->actual.max_msg_size);
	}

	if (set_me_data) {
		info->me_data = get_datatype(info->type, data_val);
		set_data(info->me_data, info->buf, info->type,
			info->actual.max_msg_size);
	}
	return 0;
}

static int check_data(struct node_info *info, char *val, void *data, int type, int length)
{
	uint8_t *p_u8;
	int8_t *p_8;
	uint16_t *p_u16;
	int16_t *p_16;
	uint32_t *p_u32;
	int32_t *p_32;
	uint64_t *p_u64;
	int64_t *p_64;
	float *p_f;
	float complex *p_fc;
	double *p_d;
	double complex *p_dc;
	long double *p_ld;
	long double complex *p_ldc;
	int i;
	datatype_t num;
	float eps = 1e-6;
	double deps = 1e-12;

	num = get_datatype(type, val);

	switch(type) {
	case PTL_INT8_T:
		p_8 = data;
		for (i = 0; i < length; i++, p_8++)
			if (*p_8 != num.s8) {
				if (debug)
					printf("check_data char failed expected %x got %x at i = %d\n",
						num.s8, *p_8, i);
				return 1;
			}
		break;
	case PTL_UINT8_T:
		p_u8 = data;
		for (i = 0; i < length; i++, p_u8++)
			if (*p_u8 != num.u8) {
				if (debug)
					printf("check_data uchar failed expected %x got %x at i = %d\n",
						num.u8, *p_u8, i);
				return 1;
			}
		break;
	case PTL_INT16_T:
		p_16 = data;
		for (i = 0; i < length/2; i++, p_16++)
			if (*p_16 != num.s16) {
				if (debug)
					printf("check_data short failed expected %x got %x at i = %d\n",
						num.s16, *p_16, i);
				return 1;
			}
		break;
	case PTL_UINT16_T:
		p_u16 = data;
		for (i = 0; i < length/2; i++, p_u16++)
			if (*p_u16 != num.u16) {
				if (debug)
					printf("check_data ushort failed expected %x got %x at i = %d\n",
						num.u16, *p_u16, i);
				return 1;
			}
		break;
	case PTL_INT32_T:
		p_32 = data;
		for (i = 0; i < length/4; i++, p_32++)
			if (*p_32 != num.s32) {
				if (debug)
					printf("check_data int failed expected %x got %x at i = %d\n",
						num.s32, *p_32, i);
				return 1;
			}
		break;
	case PTL_UINT32_T:
		p_u32 = data;
		for (i = 0; i < length/4; i++, p_u32++)
			if (*p_u32 != num.u32) {
				if (debug)
					printf("check_data uint failed expected %x got %x at i = %d\n",
						num.u32, *p_u32, i);
				return 1;
			}
		break;
	case PTL_INT64_T:
		p_64 = data;
		for (i = 0; i < length/8; i++, p_64++)
			if (*p_64 != num.s64) {
				if (debug)
					printf("check_data long failed "
						"expected %" PRIx64 "got %"
						PRIx64 " at i = %d\n",
						num.s64, *p_64, i);
				return 1;
			}
		break;
	case PTL_UINT64_T:
		p_u64 = data;
		for (i = 0; i < length/8; i++, p_u64++)
			if (*p_u64 != num.u64) {
				if (debug)
					printf("check_data ulong failed "
						"expected %" PRIx64 " got %"
						PRIx64 " at i = %d\n",
						num.u64, *p_u64, i);
				return 1;
			}
		break;
	case PTL_FLOAT:
		p_f = data;
		for (i = 0; i < length/4; i++, p_f++)
			if (*p_f > (num.f + eps) || *p_f < (num.f - eps)) {
				if (debug)
					printf("check_data float failed expected %12.10f got %12.10f at i = %d\n",
						num.f, *p_f, i);
				return 1;
			}
		break;
	case PTL_FLOAT_COMPLEX:
		p_fc = data;
		for (i = 0; i < length/8; i++, p_fc ++) {
			if (crealf(*p_fc) > (crealf(num.fc) + eps) || crealf(*p_fc) < (crealf(num.fc) - eps)) {
				if (debug)
					printf("check_data complex.re failed expected %12.10f got %12.10f at i = %d\n",
						   crealf(num.fc), crealf(*p_fc), i);
				return 1;
			}
			if (cimagf(*p_fc) > (cimagf(num.fc) + eps) || cimagf(*p_fc) < (cimagf(num.fc) - eps)) {
				if (debug)
					printf("check_data complex.im failed expected %12.10f got %12.10f at i = %d\n",
						   cimagf(num.fc), cimagf(*p_fc), i);
				return 1;
			}
		}
		break;
	case PTL_DOUBLE:
		p_d = data;
		for (i = 0; i < length/8; i++, p_d++)
			if (*p_d > (num.d + deps) || *p_d < (num.d - deps)) {
				if (debug)
					printf("check_data double failed expected %22.20lf got %22.20lf at i = %d\n",
						num.d, *p_d, i);
				return 1;
			}
		break;
	case PTL_DOUBLE_COMPLEX:
		p_dc = data;
		for (i = 0; i < length/16; i++, p_dc ++) {
			if (creal(*p_dc) > (creal(num.dc) + deps) || creal(*p_dc) < (creal(num.dc) - deps)) {
				if (debug)
					printf("check_data complex.re failed expected %22.20lf got %22.20lf at i = %d\n",
						   creal(num.dc), creal(*p_dc), i);
				return 1;
			}
			if (cimag(*p_dc) > (cimag(num.dc) + deps) || cimag(*p_dc) < (cimag(num.dc) - deps)) {
				if (debug)
					printf("check_data complex.im failed expected %22.20lf got %22.20lf at i = %d\n",
						   cimag(num.dc), cimag(*p_dc), i);
				return 1;
			}
		}
		break;
	case PTL_LONG_DOUBLE:
		p_ld = data;
		for (i = 0; i < length/sizeof(*p_ld); i++, p_ld ++)
			if (*p_ld > (num.ld + deps) || *p_ld < (num.ld - deps)) {
				if (debug)
					printf("check_data long double failed expected %22.20Lf got %22.20Lf at i = %d\n",
						num.ld, *p_ld, i);
				return 1;
			}
		break;
	case PTL_LONG_DOUBLE_COMPLEX:
		p_ldc = data;
		for (i = 0; i < length/sizeof(*p_ldc); i++, p_ldc ++) {
			if (creall(*p_ldc) > (creall(num.ldc) + deps) || creall(*p_ldc) < (creall(num.ldc) - deps)) {
				if (debug)
					printf("check_data complex.re failed expected %22.20Lf got %22.20Lf at i = %d\n",
						   creall(num.ldc), creall(*p_ldc), i);
				return 1;
			}
			if (cimagl(*p_ldc) > (cimagl(num.ldc) + deps) || cimagl(*p_ldc) < (cimagl(num.ldc) - deps)) {
				if (debug)
					printf("check_data complex.im failed expected %22.20Lf got %22.20Lf at i = %d\n",
						   cimagl(num.ldc), cimagl(*p_ldc), i);
				return 1;
			}
		}
		break;
	default:
		return 1;
	}

	return 0;
}

static int check_attr(struct node_info *info, xmlNode *node)
{
	xmlAttr *attr;
	char *val;
	struct dict_entry *e;
	unsigned int offset = 0;
	unsigned int length = 1;
	int type = PTL_UINT8_T;
	int do_check_data = 0;
	char *check_data_val = check_data_val;

	for (attr = node->properties; attr; attr = attr->next) {
		val = (char *)attr->children->content;
		e = lookup((char *)attr->name);
		if (!e) {
			printf("invalid attr: %s\n", attr->name);
			return 1;
		}

		switch (e->token) {
		case ATTR_ERR:
			if(info->err != get_ret(val)) {
				return 1;
			}
			break;
		case ATTR_THREAD_ID:
			if(info->thread_id != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_ENTRIES:
			if(info->actual.max_entries != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_MDS:
			if(info->actual.max_mds != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_EQS:
			if(info->actual.max_eqs != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_CTS:
			if(info->actual.max_cts != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_IOVECS:
			if(info->actual.max_iovecs != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_LIST_SIZE:
			if(info->actual.max_list_size != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_MSG_SIZE:
			if(info->actual.max_msg_size != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_ATOMIC_SIZE:
			if(info->actual.max_atomic_size != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_MAX_PT_INDEX:
			if(info->actual.max_pt_index != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_ACTUAL_FEATURES:
			if(info->actual.features != get_ni_features(val)) {
				return 1;
			}
			break;
		case ATTR_WHICH:
			if(info->which != get_number(info, val)) {
				return 1;
			}
			break;

		case ATTR_EVENT_TYPE:
			if (info->eq_event.type != get_event_type(val))
				return 1;
			break;
		case ATTR_EVENT_NID:
			if (info->eq_event.initiator.phys.nid != get_number(info, val))
				return 1;
			break;
		case ATTR_EVENT_PID:
			if (info->eq_event.initiator.phys.pid != get_number(info, val))
				return 1;
			break;
		case ATTR_EVENT_RANK:
			if (info->eq_event.initiator.rank != get_number(info, val))
				return 1;
			break;
		case ATTR_EVENT_PT_INDEX:
			if (info->eq_event.pt_index != get_number(info, val))
				return 1;
			break;
		case ATTR_EVENT_UID:
			if (info->eq_event.uid != get_number(info, val))
				return 1;
			break;
		case ATTR_EVENT_MATCH:
			if(info->eq_event.match_bits != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_EVENT_RLENGTH:
			if(info->eq_event.rlength != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_EVENT_MLENGTH: {
			ptl_size_t mlength;
			/* TODO what is the point of this and similar below ??? */
			if (info->eq_event.type == PTL_EVENT_REPLY ||
			    info->eq_event.type == PTL_EVENT_SEND ||
			    info->eq_event.type == PTL_EVENT_ACK)
				mlength = info->eq_event.mlength;
			else
				mlength = info->eq_event.mlength;
			if(mlength != get_number(info, val)) {
				return 1;
			}
			break;
		}
		case ATTR_EVENT_OFFSET: {
			ptl_size_t offset;
			if (info->eq_event.type == PTL_EVENT_REPLY ||
			    info->eq_event.type == PTL_EVENT_SEND ||
			    info->eq_event.type == PTL_EVENT_ACK)
				offset = info->eq_event.remote_offset;
			else
				offset = info->eq_event.remote_offset;
			if(offset != get_number(info, val)) {
				return 1;
			}
			break;
		}
		case ATTR_EVENT_START:
			if(info->eq_event.start != get_ptr(val)) {
				return 1;
			}
			break;
		case ATTR_EVENT_USER_PTR: {
			void *user_ptr;
			if (info->eq_event.type == PTL_EVENT_REPLY ||
			    info->eq_event.type == PTL_EVENT_SEND ||
			    info->eq_event.type == PTL_EVENT_ACK)
				user_ptr = info->eq_event.user_ptr;
			else
				user_ptr = info->eq_event.user_ptr;
			if(user_ptr != get_ptr(val)) {
				return 1;
			}
			break;
		}
		case ATTR_EVENT_HDR_DATA:
			if(info->eq_event.hdr_data != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_EVENT_NI_FAIL: {
			int ni_fail;
			if (info->eq_event.type == PTL_EVENT_REPLY ||
			    info->eq_event.type == PTL_EVENT_SEND ||
			    info->eq_event.type == PTL_EVENT_ACK)
				ni_fail = info->eq_event.ni_fail_type;
			else
				ni_fail = info->eq_event.ni_fail_type;
			if(ni_fail != get_ni_fail(val)) {
				return 1;
			}
			break;
		}
		case ATTR_EVENT_ATOM_OP:
			if(info->eq_event.atomic_operation != get_atom_op(val)) {
				return 1;
			}
			break;
		case ATTR_EVENT_ATOM_TYPE:
			if(info->eq_event.atomic_type != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_EVENT_PTL_LIST:
			if(info->eq_event.ptl_list != get_list(val)) {
				return 1;
			}
			break;
		case ATTR_CT_EVENT_SUCCESS:
			if(info->ct_event.success != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_CT_EVENT_FAILURE:
			if(info->ct_event.failure != get_number(info, val)) {
				return 1;
			}
			break;
		case ATTR_LENGTH:
			length = get_number(info, val);
			break;
		case ATTR_OFFSET:
			offset = get_number(info, val);
			break;
		case ATTR_TYPE:
			type = get_atom_type(val);
			break;
		case ATTR_MD_DATA:
		case ATTR_LE_DATA:
		case ATTR_ME_DATA:
			do_check_data = 1;
			check_data_val = val;
			break;
		case ATTR_RANK:
			if (info->id.rank != get_number(info, val)) {
				return 1;
			}
			break;
		default:
			printf("unexpected check attribute: %s\n", e->name);
			return 1;
		}
	}

	if (do_check_data &&
		check_data(info, check_data_val, &info->buf[offset], type, length))
		return 1;

	return 0;
}

static int check_opt(xmlNode *node)
{
	xmlAttr *attr;
	struct dict_entry *e;

	for (attr = node->properties; attr; attr = attr->next) {
		e = lookup((char *)attr->name);
		if (!e)
			return 0;
	}

	return 1;
}

static int walk_tree(struct node_info *head, xmlNode *parent);

static void *run_thread(void *arg)
{
	volatile struct thread_info *t = (volatile struct thread_info *)arg;
	xmlNode *node = t->node;
	struct node_info *info;

	do {
		sched_yield();
	} while(!t->run);

	info = malloc(sizeof(*info));
	if (!info) {
		printf("unable to allocate memory for info\n");
		t->errs++;
		return NULL;
	}
	*info = *t->info;
	info->thread_id = t->num;

	t->errs = walk_tree(info, node);

	free(info);

	return NULL;
}

int skipped;

static int walk_tree(struct node_info *info, xmlNode *parent)
{
	xmlNode *node = NULL;
	int errs;
	int tot_errs = 0;
	int i;
	struct dict_entry *e;

	for (node = parent; node; node = node->next) {
		if (node->type == XML_ELEMENT_NODE) {
			errs = 0;
			e = lookup((char *)node->name);
			if (!e) {
				errs = 1;
				printf("invalid node: %s\n", node->name);
				goto done;
			}

			if (debug) printf("rank %d: start node = %s\n", libtest_get_rank(), e->name);

			/* the following cases do not push the stack */
			switch (e->token) {
			case NODE_SET:
				errs = get_attr(info, node);
				goto done;
			case NODE_CHECK:
				errs = check_attr(info, node);
				goto done;
			case NODE_IF:
				if ((info->cond = (check_attr(info, node) == PTL_OK)))
					errs = walk_tree(info, node->children);
				goto done;
			case NODE_ELSE:
				if (!info->cond)
					errs = walk_tree(info, node->children);
				goto done;
			case NODE_ARG_CHECK:
#ifdef NO_ARG_VALIDATION
				errs = 0;
				skipped++;
#else
				errs = walk_tree(info, node->children);
#endif
				goto done;
			case NODE_IFDEF:
				if (check_opt(node))
					errs = walk_tree(info, node->children);
				goto done;
			case NODE_DESC:
				/* printf("%-60s", node->children->content);
				   fflush(stdout); */
				goto done;
			case NODE_COMMENT:
				goto done;

			case NODE_BARRIER:
				libtest_barrier();
				goto done;
			}

			info = push_info(info, e->token);
			errs = get_attr(info, node);
			if (errs)
				goto pop;

			switch (e->token) {
			case NODE_TEST:
				errs = walk_tree(info, node->children);
				break;
			case NODE_REPEAT:
				for (i = 0; i < info->count; i++)
					errs += walk_tree(info, node->children);
				break;
			case NODE_THREADS: {
				volatile struct thread_info *t;
				info->threads = calloc(info->count, sizeof(struct thread_info));
				if (!info->threads) {
					printf("unable to allocate memory for info->threads\n");
					exit(1);
				}

				for (i = 0; i < info->count; i++) {
					t = &info->threads[i];
					t->num = i;
					t->node = node->children;
					t->info = info;
					pthread_create((pthread_t *)&t->thread, NULL, run_thread, (void *)t);
				}

				for (i = 0; i < info->count; i++)
					info->threads[i].run = 1;

				for (i = 0; i < info->count; i++) {
					void *ignore;
					t = &info->threads[i];
					pthread_join(t->thread, &ignore);
					errs += t->errs;
				}

				free((void *)info->threads);
				break;
			}
			case NODE_MSLEEP:
				usleep(1000*info->count);
				errs += walk_tree(info, node->children);
				break;
			case NODE_TIME: {
				struct timeval start_time;
				struct timeval stop_time;
				double diff;

       				gettimeofday(&start_time, NULL);
				errs += walk_tree(info, node->children);
       				gettimeofday(&stop_time, NULL);

				diff = 1e-6*(stop_time.tv_usec - start_time.tv_usec)
					+ (stop_time.tv_sec - start_time.tv_sec);

				printf(" [time = %.6lf] ", diff);
				break;
			}
			case NODE_OMPI_RT:
				errs = ompi_rt_init(info);
				errs += walk_tree(info, node->children);
				errs += ompi_rt_fini(info);
				break;
			case NODE_PTL:
				errs = test_ptl_init(info);
				errs += walk_tree(info, node->children);
				test_ptl_fini(info);
				break;
			case NODE_PTL_INIT:
				errs = test_ptl_init(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_FINI:
				test_ptl_fini(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_NI:
				errs = test_ptl_ni_init(info);
				errs += walk_tree(info, node->children);
				errs += test_ptl_ni_fini(info);
				break;
			case NODE_PTL_NI_INIT:
				errs = test_ptl_ni_init(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_NI_FINI:
				errs = test_ptl_ni_fini(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_NI_STATUS:
				errs = test_ptl_ni_status(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_NI_HANDLE:
				errs = test_ptl_ni_handle(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_PT:
				for (i = 0; i < info->count - 1; i++) {
					errs += test_ptl_pt_alloc(info);
					info = push_info(info, e->token);
					errs += get_attr(info, node);
				}
				errs += test_ptl_pt_alloc(info);
				errs += walk_tree(info, node->children);
				errs += test_ptl_pt_free(info);
				for (i = 0; i < info->count - 1; i++) {
					info = pop_node(info);
					errs += test_ptl_pt_free(info);
				}
				break;
			case NODE_PTL_PT_ALLOC:
				errs = test_ptl_pt_alloc(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_PT_FREE:
				errs = test_ptl_pt_free(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_PT_DISABLE:
				errs = test_ptl_pt_disable(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_PT_ENABLE:
				errs = test_ptl_pt_enable(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_GET_ID:
				errs = test_ptl_get_id(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_GET_UID:
				errs = test_ptl_get_uid(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_MD:
				for (i = 0; i < info->count - 1; i++) {
					errs += test_ptl_md_bind(info);
					info = push_info(info, e->token);
					errs += get_attr(info, node);
				}
				errs += test_ptl_md_bind(info);
				errs += walk_tree(info, node->children);
				errs += test_ptl_md_release(info);
				for (i = 0; i < info->count - 1; i++) {
					info = pop_node(info);
					errs += test_ptl_md_release(info);
				}
				break;
			case NODE_PTL_MD_BIND:
				errs = test_ptl_md_bind(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_MD_RELEASE:
				errs = test_ptl_md_release(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_LE:
				for (i = 0; i < info->count - 1; i++) {
					errs += test_ptl_le_append(info);
					info = push_info(info, e->token);
					errs += get_attr(info, node);
				}
				errs += test_ptl_le_append(info);
				errs += walk_tree(info, node->children);
				errs += test_ptl_le_unlink(info);
				for (i = 0; i < info->count - 1; i++) {
					info = pop_node(info);
					errs += test_ptl_le_unlink(info);
				}
				break;
			case NODE_PTL_LE_APPEND:
				errs = test_ptl_le_append(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_LE_UNLINK:
				errs = test_ptl_le_unlink(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_LE_SEARCH:
				errs = test_ptl_le_search(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_ME:
				for (i = 0; i < info->count - 1; i++) {
					errs += test_ptl_me_append(info);
					info = push_info(info, e->token);
					errs += get_attr(info, node);
				}
				errs += test_ptl_me_append(info);
				errs += walk_tree(info, node->children);
				errs += test_ptl_me_unlink(info);
				for (i = 0; i < info->count - 1; i++) {
					info = pop_node(info);
					errs += test_ptl_me_unlink(info);
				}
				break;
			case NODE_PTL_ME_APPEND:
				errs = test_ptl_me_append(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_ME_UNLINK:
				errs = test_ptl_me_unlink(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_ME_SEARCH:
				errs = test_ptl_me_search(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_EQ:
				for (i = 0; i < info->count - 1; i++) {
					errs += test_ptl_eq_alloc(info);
					info = push_info(info, e->token);
					errs += get_attr(info, node);
				}
				errs += test_ptl_eq_alloc(info);
				errs += walk_tree(info, node->children);
				errs += test_ptl_eq_free(info);
				for (i = 0; i < info->count - 1; i++) {
					info = pop_node(info);
					errs += test_ptl_eq_free(info);
				}
				break;
			case NODE_PTL_EQ_ALLOC:
				errs = test_ptl_eq_alloc(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_EQ_FREE:
				errs = test_ptl_eq_free(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_EQ_GET:
				errs = test_ptl_eq_get(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_EQ_WAIT:
				errs = test_ptl_eq_wait(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_EQ_POLL:
				errs = test_ptl_eq_poll(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT:
				for (i = 0; i < info->count - 1; i++) {
					errs += test_ptl_ct_alloc(info);
					info = push_info(info, e->token);
					errs += get_attr(info, node);
				}
				errs += test_ptl_ct_alloc(info);
				errs += walk_tree(info, node->children);
				errs += test_ptl_ct_free(info);
				for (i = 0; i < info->count - 1; i++) {
					info = pop_node(info);
					errs += test_ptl_ct_free(info);
				}
				break;
			case NODE_PTL_CT_ALLOC:
				errs = test_ptl_ct_alloc(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT_FREE:
				errs = test_ptl_ct_free(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT_GET:
				errs = test_ptl_ct_get(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT_WAIT:
				errs = test_ptl_ct_wait(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT_POLL:
				errs = test_ptl_ct_poll(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT_SET:
				errs = test_ptl_ct_set(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT_INC:
				errs = test_ptl_ct_inc(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_CT_CANCEL_TRIG:
				errs = test_ptl_ct_cancel_trig(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_PUT:
				errs = test_ptl_put(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_GET:
				errs = test_ptl_get(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_ATOMIC:
				errs = test_ptl_atomic(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_ATOMIC_SYNC:
				errs = test_ptl_atomic_sync(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_FETCH:
				errs = test_ptl_fetch_atomic(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_SWAP:
				errs = test_ptl_swap(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_TRIG_PUT:
				errs = test_ptl_trig_put(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_TRIG_GET:
				errs = test_ptl_trig_get(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_TRIG_ATOMIC:
				errs = test_ptl_trig_atomic(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_TRIG_FETCH:
				errs = test_ptl_trig_fetch_atomic(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_TRIG_SWAP:
				errs = test_ptl_trig_swap(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_TRIG_CT_INC:
				errs = test_ptl_trig_ct_inc(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_TRIG_CT_SET:
				errs = test_ptl_trig_ct_set(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_START_BUNDLE:
				errs = test_ptl_start_bundle(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_END_BUNDLE:
				errs = test_ptl_end_bundle(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_HANDLE_IS_EQUAL:
				errs = test_ptl_handle_is_eq(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_SET_MAP:
				errs = test_ptl_set_map(info);
				errs += walk_tree(info, node->children);
				break;
			case NODE_PTL_GET_MAP:
				errs = test_ptl_get_map(info);
				errs += walk_tree(info, node->children);
				break;
			}
pop:
			info = pop_node(info);
done:
			tot_errs += errs;
			if (debug && e)
				printf("rank %d: end node = %s, errs = %d\n", libtest_get_rank(), e->name, errs);
		}
	}

	return tot_errs;
}

static void set_default_info(struct node_info *info)
{
	memset(info, 0, sizeof(*info));

	info->count				= 1;
	info->ret				= PTL_OK;

	info->iface				= PTL_IFACE_DEFAULT;
	info->pid				= PTL_PID_ANY;
	info->rank				= PTL_RANK_ANY;
	info->ni_opt				= PTL_NI_MATCHING | PTL_NI_PHYSICAL;
	info->desired.max_entries		= 15;
	info->desired.max_mds			= 10;
	info->desired.max_cts			= 10;
	info->desired.max_eqs			= 10;
	info->desired.max_pt_index		= 63;
	info->desired.max_iovecs		= IOV_SIZE;
	info->desired.max_list_size		= 10;
	info->desired.max_msg_size		= IOV_SIZE*IOVEC_LENGTH;
	info->desired.max_atomic_size		= 64;
	info->map_size				= 10;
	info->ni_handle				= PTL_INVALID_HANDLE;
	info->handle				= PTL_INVALID_HANDLE;
	info->iovec_length			= IOVEC_LENGTH;

	info->timeout				= 10;	/* msec */
	info->eq_count				= 10;
	info->eq_size				= 1;
	info->eq_handle				= PTL_EQ_NONE;
	info->ct_size				= 1;
	info->ct_handle				= PTL_CT_NONE;
	info->atom_op				= PTL_SUM;
	info->list				= PTL_PRIORITY_LIST;

	info->user_ptr				= NULL;
	info->uid					= PTL_UID_ANY;

	info->length				= 1;
	info->loc_offset			= 0;
	info->ack_req				= PTL_NO_ACK_REQ;
}

int run_doc(xmlDocPtr doc)
{
	int errs;
	struct node_info *info;
	xmlNode *root_element;

	info = malloc(sizeof(*info));
	if (!info) {
		printf("unable to allocate memory for node info\n");
		return 1;
	}

	set_default_info(info);

	root_element = xmlDocGetRootElement(doc);

	errs = walk_tree(info, root_element);

	free(info);

	if (errs != 0)
		printf("	Total Errors %d\n\n", errs);

	return errs != 0;
}

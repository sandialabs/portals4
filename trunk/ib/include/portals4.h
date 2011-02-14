/*
 * portals4.h
 */

#ifndef PORTALS4_H
#define PORTALS4_H

/*
 * All symbold beginning with underscore are private to the implementation
 * for the convenience of the implementation developers and can be ignored
 * by applications.
 *
 * All references to 3.xx.y are to sections of "Portals 4.0 Message
 * Passing API". This version of portals4.h corresponds to Draft 11/11/2010
 * of that document.
 */

/* private return value from APIs that do not normally return errors
 * for testing purposes */
extern int			ptl_test_return;

/* private control for logging output */
extern int			ptl_log_level;

/*---------------------------------------------------------------------------*
 * Data Types and Constants
 *---------------------------------------------------------------------------*/

typedef uint64_t		ptl_size_t;		/* 3.2.1 */

typedef uint64_t		ptl_handle_any_t;	/* 3.2.2 */
typedef ptl_handle_any_t	ptl_handle_ni_t;
typedef ptl_handle_any_t	ptl_handle_pt_t;
typedef ptl_handle_any_t	ptl_handle_le_t;
typedef ptl_handle_any_t	ptl_handle_me_t;
typedef ptl_handle_any_t	ptl_handle_md_t;
typedef ptl_handle_any_t	ptl_handle_eq_t;
typedef ptl_handle_any_t	ptl_handle_ct_t;

enum {
	PTL_HANDLE_NONE		= 0xff00000000000000ULL,
	PTL_INVALID_HANDLE	= 0xffffffffffffffffULL,
	PTL_EQ_NONE		= PTL_HANDLE_NONE,
	PTL_CT_NONE		= PTL_HANDLE_NONE,
};

typedef unsigned int		ptl_pt_index_t;		/* 3.2.3 */

enum {
	PTL_PT_ANY		= 0xff000001U,
};

typedef uint64_t		ptl_match_bits_t;	/* 3.2.4 */

typedef unsigned int		ptl_interface_t;	/* 3.2.5 */

enum {
	PTL_IFACE_DEFAULT	= 0xff000002U,
};

typedef unsigned int		ptl_nid_t;		/* 3.2.6 */
typedef unsigned int		ptl_pid_t;
typedef unsigned int		ptl_rank_t;
typedef unsigned int		ptl_uid_t;
typedef unsigned int		ptl_jid_t;

enum {
	PTL_NID_ANY		= 0xff000003U,
	PTL_PID_ANY		= 0xff000004U,
	PTL_RANK_ANY		= 0xff000005U,
	PTL_UID_ANY		= 0xff000006U,
	PTL_JID_ANY		= 0xff000007U,
	PTL_JID_NONE		= 0xff000000U,
};

typedef enum {						/* 3.2.7 */
	PTL_SR_DROP_COUNT,
	PTL_SR_PERMISSIONS_VIOLATIONS,
	_PTL_SR_LAST,
} ptl_sr_index_t;

typedef int			ptl_sr_value_t;

typedef uint64_t		ptl_hdr_data_t;
typedef unsigned int		ptl_time_t;

enum {							/* 3.13, 3.14 */
	PTL_TIME_FOREVER	= 0xffffffff
};

enum ptl_retvals {					/* 3.3 */
	PTL_OK			= 0,
	PTL_FAIL		= 1,
	PTL_ARG_INVALID,
	PTL_CT_NONE_REACHED,
	PTL_EQ_DROPPED,
	PTL_EQ_EMPTY,
	PTL_IN_USE,
	PTL_INTERRUPTED,
	PTL_LIST_TOO_LONG,
	PTL_NO_INIT,
	PTL_NO_SPACE,
	PTL_PID_IN_USE,
	PTL_PT_FULL,
	PTL_PT_EQ_NEEDED,
	PTL_PT_IN_USE,
	PTL_SIZE_INVALID,
	_PTL_STATUS_LAST,	/* keep me last */
};

typedef struct {					/* 3.5.1 */
	int			max_entries;
	int			max_overflow_entries;
	int			max_mds;
	int			max_cts;
	int			max_eqs;
	int			max_pt_index;
	int			max_iovecs;
	int			max_list_size;
	ptl_size_t		max_msg_size;
	ptl_size_t		max_atomic_size;
	ptl_size_t		max_unordered_size;
} ptl_ni_limits_t;

enum {							/* 3.5.2 */
	PTL_NI_MATCHING			= 1,
	PTL_NI_NO_MATCHING		= 1<<1,
	PTL_NI_LOGICAL			= 1<<2,
	PTL_NI_PHYSICAL			= 1<<3,
	_PTL_NI_INIT_OPTIONS		= (1<<4) - 1,
};

enum {							/* 3.6.1 */
	PTL_PT_ONLY_USE_ONCE		= 1,
	PTL_PT_FLOWCTRL			= 1<<1,
	_PTL_PT_ALLOC_OPTIONS		= (1<<2) - 1,
};

typedef union {						/* 3.8.1 */
	struct {
		ptl_nid_t		nid;
		ptl_pid_t		pid;
	} phys;
	ptl_rank_t		rank;
} ptl_process_t;

typedef struct {					/* 3.10.1 */
	void			*start;
	ptl_size_t		length;
	unsigned int		options;
	ptl_handle_eq_t		eq_handle;
	ptl_handle_ct_t		ct_handle;
} ptl_md_t;

enum {
	PTL_IOVEC			= 1,
	PTL_MD_EVENT_SUCCESS_DISABLE	= 1<<2,
	PTL_MD_EVENT_CT_SEND		= 1<<3,
	PTL_MD_EVENT_CT_REPLY		= 1<<4,
	PTL_MD_EVENT_CT_ACK		= 1<<5,
	PTL_MD_EVENT_CT_BYTES		= 1<<6,
	PTL_MD_UNORDERED		= 1<<7,
	PTL_MD_REMOTE_FAILURE_DISABLE	= 1<<8,
	_PTL_MD_BIND_OPTIONS		= (1<<9) - 1,
};

typedef struct {					/* 3.10.2 */
	void			*iov_base;
	ptl_size_t		iov_len;
} ptl_iovec_t;

typedef union {						/* 3.11.1 */
	ptl_jid_t		jid;
	ptl_uid_t		uid;
} ptl_ac_id_t;

typedef struct {
	void			*start;
	ptl_size_t		length;
	ptl_handle_ct_t 	ct_handle;
	ptl_ac_id_t		ac_id;
	unsigned int		options;
} ptl_le_t;

enum {
	PTL_LE_OP_PUT			= 1<<1,
	PTL_LE_OP_GET			= 1<<2,
	PTL_LE_USE_ONCE			= 1<<3,
	PTL_LE_ACK_DISABLE		= 1<<4,
	PTL_LE_EVENT_COMM_DISABLE	= 1<<5,
	PTL_LE_EVENT_FLOWCTRL_DISABLE	= 1<<6,
	PTL_LE_EVENT_SUCCESS_DISABLE	= 1<<7,
	PTL_LE_EVENT_OVER_DISABLE	= 1<<8,
	PTL_LE_EVENT_UNLINK_DISABLE	= 1<<9,
	PTL_LE_EVENT_CT_COMM		= 1<<10,
	PTL_LE_EVENT_CT_OVERFLOW	= 1<<11,
	PTL_LE_EVENT_CT_BYTES		= 1<<12,
	PTL_LE_AUTH_USE_JID		= 1<<13,
	_PTL_LE_APPEND_OPTIONS		= (1<<14) - 1,
};

typedef enum {						/* 3.11.2 */
	PTL_PRIORITY_LIST,
	PTL_OVERFLOW,
	PTL_PROBE_ONLY,
	_PTL_LIST_LAST,
} ptl_list_t;

typedef struct {					/* 3.12.1 */
	void			*start;
	ptl_size_t		length;
	ptl_handle_ct_t		ct_handle;
	ptl_ac_id_t		ac_id;
	unsigned int		options;
	ptl_size_t		min_free;
	ptl_process_t		match_id;
	ptl_match_bits_t	match_bits;
	ptl_match_bits_t	ignore_bits;
} ptl_me_t;

enum {
	PTL_ME_OP_PUT			= PTL_LE_OP_PUT,
	PTL_ME_OP_GET			= PTL_LE_OP_GET,
	PTL_ME_USE_ONCE			= PTL_LE_USE_ONCE,
	PTL_ME_ACK_DISABLE		= PTL_LE_ACK_DISABLE,
	PTL_ME_EVENT_COMM_DISABLE	= PTL_LE_EVENT_COMM_DISABLE,
	PTL_ME_EVENT_FLOWCTRL_DISABLE	= PTL_LE_EVENT_FLOWCTRL_DISABLE,
	PTL_ME_EVENT_SUCCESS_DISABLE	= PTL_LE_EVENT_SUCCESS_DISABLE,
	PTL_ME_EVENT_OVER_DISABLE	= PTL_LE_EVENT_OVER_DISABLE,
	PTL_ME_EVENT_UNLINK_DISABLE	= PTL_LE_EVENT_UNLINK_DISABLE,
	PTL_ME_EVENT_CT_COMM		= PTL_LE_EVENT_CT_COMM,
	PTL_ME_EVENT_CT_OVERFLOW	= PTL_LE_EVENT_CT_OVERFLOW,
	PTL_ME_EVENT_CT_BYTES		= PTL_LE_EVENT_CT_BYTES,
	PTL_ME_AUTH_USE_JID		= PTL_LE_AUTH_USE_JID,
	PTL_ME_MANAGE_LOCAL		= 1<<14,
	PTL_ME_NO_TRUNCATE		= 1<<15,
	PTL_ME_MAY_ALIGN		= 1<<16,
	_PTL_ME_APPEND_OPTIONS		= (1<<17) - 1,
};

typedef enum {						/* 3.13.1 */
	PTL_EVENT_GET,
	PTL_EVENT_PUT,
	PTL_EVENT_PUT_OVERFLOW,
	PTL_EVENT_ATOMIC,
	PTL_EVENT_ATOMIC_OVERFLOW,
	PTL_EVENT_REPLY,
	PTL_EVENT_SEND,
	PTL_EVENT_ACK,
	PTL_EVENT_PT_DISABLED,
	PTL_EVENT_AUTO_UNLINK,
	PTL_EVENT_AUTO_FREE,
	PTL_EVENT_PROBE,
	_PTL_EVENT_KIND_LAST,
} ptl_event_kind_t;

typedef enum {						/* 3.13.3 */
	PTL_NI_OK,
	PTL_NI_UNDELIVERABLE,
	PTL_NI_DROPPED,
	PTL_NI_FLOW_CTRL,
	PTL_NI_PERM_VIOLATION,
	_PTL_NI_FAIL_LAST,
} ptl_ni_fail_t;

typedef enum {						/* 3.15.4 */
	PTL_MIN,
	PTL_MAX,
	PTL_SUM,
	PTL_PROD,
	PTL_LOR,
	PTL_LAND,
	PTL_BOR,
	PTL_BAND,
	PTL_LXOR,
	PTL_BXOR,
	PTL_SWAP,
	PTL_CSWAP,
	PTL_CSWAP_NE,
	PTL_CSWAP_LE,
	PTL_CSWAP_LT,
	PTL_CSWAP_GE,
	PTL_CSWAP_GT,
	PTL_MSWAP,
	_PTL_OP_LAST
} ptl_op_t;

typedef enum {
	PTL_CHAR,
	PTL_UCHAR,
	PTL_SHORT,
	PTL_USHORT,
	PTL_INT,
	PTL_UINT,
	PTL_LONG,
	PTL_ULONG,
	PTL_FLOAT,
	PTL_DOUBLE,
	_PTL_DATATYPE_LAST,
} ptl_datatype_t;

typedef struct {					/* 3.13.4 */
	ptl_event_kind_t	type;
	ptl_process_t		initiator;
	ptl_pt_index_t		pt_index;
	ptl_uid_t		uid;
	ptl_jid_t		jid;
	ptl_match_bits_t	match_bits;
	ptl_size_t		rlength;
	ptl_size_t		mlength;
	ptl_size_t		remote_offset;
	void			*start;
	void			*user_ptr;
	ptl_hdr_data_t		hdr_data;
	ptl_ni_fail_t		ni_fail_type;
	ptl_op_t		atomic_operation;
	ptl_datatype_t		atomic_type;
} ptl_event_t;

typedef struct {					/* 3.14.1 */
	ptl_size_t		success;
	ptl_size_t		failure;
} ptl_ct_event_t;

typedef enum {						/* 3.15.1 */
	PTL_ACK_REQ,
	PTL_NO_ACK_REQ,
	PTL_CT_ACK_REQ,
	PTL_OC_ACK_REQ,
	_PTL_ACK_REQ_LAST,
} ptl_ack_req_t;

/*---------------------------------------------------------------------------*
 * Function Prototypes
 *---------------------------------------------------------------------------*/

int PtlInit(void);					/* 3.4.1 */

void PtlFini(void);					/* 3.4.2 */

int PtlNIInit(						/* 3.5.2 */
	ptl_interface_t		iface,
	unsigned int		options,
	ptl_pid_t		pid,
	ptl_ni_limits_t		*desired,
	ptl_ni_limits_t		*actual,
	ptl_size_t		map_size,
	ptl_process_t		*desired_mapping,
	ptl_process_t		*actual_mapping,
	ptl_handle_ni_t		*ni_handle);

int PtlNIFini(						/* 3.5.3 */
	ptl_handle_ni_t		ni_handle);

int PtlNIStatus(					/* 3.5.4 */
	ptl_handle_ni_t		ni_handle,
	ptl_sr_index_t		status_register,
	ptl_sr_value_t		*status);

int PtlNIHandle(					/* 3.5.5 */
	ptl_handle_any_t	handle,
	ptl_handle_ni_t		*ni_handle);

int PtlPTAlloc(						/* 3.6.1 */
	ptl_handle_ni_t		ni_handle,
	unsigned int		options,
	ptl_handle_eq_t		eq_handle,
	ptl_pt_index_t		pt_index_req,
	ptl_pt_index_t		*pt_index);

int PtlPTFree(						/* 3.6.2 */
	ptl_handle_ni_t		ni_handle,
	ptl_pt_index_t		pt_index);

int PtlPTDisable(					/* 3.6.3 */
	ptl_handle_ni_t		ni_handle,
	ptl_pt_index_t		pt_index);

int PtlPTEnable(					/* 3.6.4 */
	ptl_handle_ni_t		ni_handle,
	ptl_pt_index_t		pt_index);

int PtlGetUid(						/* 3.7.1 */
	ptl_handle_ni_t		ni_handle,
	ptl_uid_t		*uid);

int PtlGetId(						/* 3.8.2 */
	ptl_handle_ni_t		ni_handle,
	ptl_process_t		*id);

int PtlGetJid(						/* 3.9.1 */
	ptl_handle_ni_t		ni_handle,
	ptl_jid_t		*jid);

int PtlMDBind(						/* 3.10.3 */
	ptl_handle_ni_t		ni_handle,
	ptl_md_t		*md,
	ptl_handle_md_t		*md_handle);

int PtlMDRelease(					/* 3.10.4 */
	ptl_handle_md_t 	md_handle);

int PtlLEAppend(					/* 3.11.2 */
	ptl_handle_ni_t		ni_handle,
	ptl_pt_index_t		pt_index,
	ptl_le_t		*le,
	ptl_list_t		ptl_list,
	void			*user_ptr,
	ptl_handle_le_t		*le_handle);

int PtlLEUnlink(					/* 3.11.3 */
	ptl_handle_le_t 	le_handle);

int PtlMEAppend(					/* 3.12.2 */
	ptl_handle_ni_t		ni_handle,
	ptl_pt_index_t		pt_index,
	ptl_me_t		*me,
	ptl_list_t		ptl_list,
	void			*user_ptr,
	ptl_handle_me_t 	*me_handle);

int PtlMEUnlink(					/* 3.12.3 */
	ptl_handle_me_t 	me_handle);

int PtlEQAlloc(						/* 3.13.5 */
	ptl_handle_ni_t		ni_handle,
	ptl_size_t		count,
	ptl_handle_eq_t 	*eq_handle);

int PtlEQFree(						/* 3.13.6 */
	ptl_handle_eq_t 	eq_handle);

int PtlEQGet(						/* 3.13.7 */
	ptl_handle_eq_t		eq_handle,
	ptl_event_t		*event);

int PtlEQWait(						/* 3.13.8 */
	ptl_handle_eq_t		eq_handle,
	ptl_event_t		*event);

int PtlEQPoll(						/* 3.13.9 */
	ptl_handle_eq_t 	*eq_handles,
	int			size,
	ptl_time_t		timeout,
	ptl_event_t		*event,
	int			*which);

int PtlCTAlloc(						/* 3.14.2 */
	ptl_handle_ni_t		ni_handle,
	ptl_handle_ct_t 	*ct_handle);

int PtlCTFree(						/* 3.14.3 */
	ptl_handle_ct_t 	ct_handle);

int PtlCTGet(						/* 3.14.4 */
	ptl_handle_ct_t		ct_handle,
	ptl_ct_event_t		*event);

int PtlCTWait(						/* 3.14.5 */
	ptl_handle_ct_t		ct_handle,
	ptl_size_t		test,
	ptl_ct_event_t		*event);

int PtlCTPoll(						/* 3.13.9 */
	ptl_handle_ct_t 	*ct_handles,
	ptl_size_t		*tests,
	int			size,
	ptl_time_t		timeout,
	ptl_ct_event_t		*event,
	int			*which);

int PtlCTSet(						/* 3.14.7 */
	ptl_handle_ct_t		ct_handle,
	ptl_ct_event_t		test);

int PtlCTInc(						/* 3.14.8 */
	ptl_handle_ct_t		ct_handle,
	ptl_ct_event_t		increment);

int PtlPut(						/* 3.15.2 */
	ptl_handle_md_t		md_handle,
	ptl_size_t		local_offset,
	ptl_size_t		length,
	ptl_ack_req_t		ack_req,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data);

int PtlGet(						/* 3.15.3 */
	ptl_handle_md_t		md_handle,
	ptl_size_t		local_offset,
	ptl_size_t		length,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	void			*user_ptr,
	ptl_size_t		remote_offset);

int PtlAtomic(						/* 3.15.4 */
	ptl_handle_md_t		md_handle,
	ptl_size_t		local_offset,
	ptl_size_t		length,
	ptl_ack_req_t		ack_req,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data,
	ptl_op_t		operation,
	ptl_datatype_t		datatype);

int PtlFetchAtomic(					/* 3.15.6 */
	ptl_handle_md_t		get_md_handle,
	ptl_size_t		local_get_offset,
	ptl_handle_md_t		put_md_handle,
	ptl_size_t		local_put_offset,
	ptl_size_t		length,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data,
	ptl_op_t		operation,
	ptl_datatype_t		datatype);

int PtlSwap(						/* 3.15.7 */
	ptl_handle_md_t		get_md_handle,
	ptl_size_t		local_get_offset,
	ptl_handle_md_t		put_md_handle,
	ptl_size_t		local_put_offset,
	ptl_size_t		length,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data,
	void			*operand,
	ptl_op_t		operation,
	ptl_datatype_t		datatype);

int PtlTriggeredPut(					/* 3.16.1 */
	ptl_handle_md_t		md_handle,
	ptl_size_t		local_offset,
	ptl_size_t		length,
	ptl_ack_req_t		ack_req,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data,
	ptl_handle_ct_t		trig_ct_handle,
	ptl_size_t		threshold);

int PtlTriggeredGet(					/* 3.16.2 */
	ptl_handle_md_t		md_handle,
	ptl_size_t		local_offset,
	ptl_size_t		length,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	void			*user_ptr,
	ptl_size_t		remote_offset,
	ptl_handle_ct_t		trig_ct_handle,
	ptl_size_t		threshold);

int PtlTriggeredAtomic(					/* 3.16.3 */
	ptl_handle_md_t		md_handle,
	ptl_size_t		local_offset,
	ptl_size_t		length,
	ptl_ack_req_t		ack_req,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data,
	ptl_op_t		operation,
	ptl_datatype_t		datatype,
	ptl_handle_ct_t		trig_ct_handle,
	ptl_size_t		threshold);

int PtlTriggeredFetchAtomic(				/* 3.16.4 */
	ptl_handle_md_t		get_md_handle,
	ptl_size_t		local_get_offset,
	ptl_handle_md_t		put_md_handle,
	ptl_size_t		local_put_offset,
	ptl_size_t		length,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data,
	ptl_op_t		operation,
	ptl_datatype_t		datatype,
	ptl_handle_ct_t		trig_ct_handle,
	ptl_size_t		threshold);

int PtlTriggeredSwap(					/* 3.16.5 */
	ptl_handle_md_t		get_md_handle,
	ptl_size_t		local_get_offset,
	ptl_handle_md_t		put_md_handle,
	ptl_size_t		local_put_offset,
	ptl_size_t		length,
	ptl_process_t		target_id,
	ptl_pt_index_t		pt_index,
	ptl_match_bits_t	match_bits,
	ptl_size_t		remote_offset,
	void			*user_ptr,
	ptl_hdr_data_t		hdr_data,
	void			*operand,
	ptl_op_t		operation,
	ptl_datatype_t		datatype,
	ptl_handle_ct_t		trig_ct_handle,
	ptl_size_t		threshold);

int PtlTriggeredCTInc(					/* 3.16.6 */
	ptl_handle_ct_t		ct_handle,
	ptl_ct_event_t		increment,
	ptl_handle_ct_t		trig_ct_threshold,
	ptl_size_t		threshold);

int PtlTriggeredCTSet(					/* 3.16.7 */
	ptl_handle_ct_t		ct_handle,
	ptl_ct_event_t		new_ct,
	ptl_handle_ct_t		trig_ct_handle,
	ptl_size_t		threshold);

int PtlStartBundle(					/* 3.17.1 */
	ptl_handle_ni_t		ni_handle);

int PtlEndBundle(					/* 3.17.2 */
	ptl_handle_ni_t		ni_handle);

int PtlHandleIsEqual(					/* 3.18.1 */
	ptl_handle_any_t	handle1,
	ptl_handle_any_t	handle2);

void _dump_type_counts();

#endif /* PORTALS4_h */

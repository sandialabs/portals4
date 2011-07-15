#ifndef PTL_PARAM_H
#define PTL_PARAM_H

enum {
	PTL_LIM_MAX_ENTRIES,
	PTL_LIM_MAX_UNEXPECTED_HEADERS,
	PTL_LIM_MAX_MDS,
	PTL_LIM_MAX_CTS,
	PTL_LIM_MAX_EQS,
	PTL_LIM_MAX_PT_INDEX,
	PTL_LIM_MAX_IOVECS,
	PTL_LIM_MAX_LIST_SIZE,
	PTL_LIM_MAX_TRIGGERED_OPS,
	PTL_LIM_MAX_MSG_SIZE,
	PTL_LIM_MAX_ATOMIC_SIZE,
	PTL_LIM_MAX_FETCH_ATOMIC_SIZE,
	PTL_LIM_MAX_WAW_ORDERED_SIZE,
	PTL_LIM_MAX_WAR_ORDERED_SIZE,
	PTL_LIM_MAX_VOLATILE_SIZE,
	PTL_LIM_FEATURES,

	PTL_OBJ_ALLOC_TIMEOUT,

	PTL_MAX_IFACE,

	PTL_MAX_QP_SEND_WR,
	PTL_MAX_QP_SEND_SGE,
	PTL_MAX_QP_RECV_SGE,
	PTL_MAX_SRQ_RECV_WR,

	PTL_MAX_RDMA_WR_OUT,

	PTL_MAX_INLINE_DATA,
	PTL_MAX_INLINE_SGE,
	PTL_MAX_INDIRECT_SGE,

	PTL_RDMA_TIMEOUT,

	PTL_PARAM_LAST,			/* keep me last */
};

typedef struct param {
	char		*name;
	long		min;
	long		max;
	long		val;
} param_t;

void init_param(int argc, char *argv[]);

long get_param(int param);

long chk_param(int param, long val);

long set_param(int param, long val);

#endif /* PTL_PARAM_H */

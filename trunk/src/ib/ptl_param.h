/**
 * @file ptl_param.h
 *
 * @brief Header file for ptl_param.c.
 */
#ifndef PTL_PARAM_H
#define PTL_PARAM_H

/**
 * @brief Parameter indices.
 */
enum {
	PTL_MAX_IFACE,
	PTL_MAX_INLINE_DATA,
	PTL_MAX_INLINE_SGE,
	PTL_MAX_INDIRECT_SGE,

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
	PTL_MAX_QP_SEND_WR,
	PTL_MAX_SEND_COMP_THRESHOLD,
	PTL_MAX_QP_SEND_SGE,
	PTL_MAX_QP_RECV_SGE,
	PTL_MAX_SRQ_RECV_WR,
	PTL_SRQ_REPOST_SIZE,
	PTL_MAX_RDMA_WR_OUT,
	PTL_RDMA_TIMEOUT,
	PTL_WC_COUNT,
	PTL_CT_WAIT_LOOP_COUNT,
	PTL_CT_POLL_LOOP_COUNT,
	PTL_EQ_WAIT_LOOP_COUNT,
	PTL_EQ_POLL_LOOP_COUNT,
	PTL_NUM_SBUF,

	PTL_LOG_LEVEL,
	PTL_DEBUG,

	PTL_PARAM_LAST,		/* keep me last */
};

/**
 * @brief Info about tunable parameters.
 */
struct param {
	/** used to match an environment variable */
	char		*name;
	/** the minimum acceptable value */
	long		min;
	/** the maximum acceptable value */
	long		max;
	/** the current/default value of the parameter */
	long		val;
};

typedef struct param param_t;

extern param_t param[];

/*
 * This is used once by the library to initialize the parameter
 * array and must be called during PtlInit before any other param
 * calls.
 */
void init_param(void);

/*
 * This is used by the library for parameters that can be set by the
 * API when the caller does try to set them. They are checked against
 * the upper and lower limits and modified if out of bounds. The
 * default value is not changed by this call. For example two calls
 * to PtlNIInit one with and one without desired limits will result in
 * the original defaults on the second call.
 */
long chk_param(int param, long val);

/*
 * This call is currently not used but could be used to modify the
 * default values in the array. For example because of some interaction
 * with a runtime interface.
 */
long set_param(int parm, long val);

/**
 * @brief Return the current value of a parameter.
 *
 * This routine is used by the library for tunable parameters that are
 * not set through the API. For example the number of InfiniBand send
 * work requests. It is also used as the default value for user settable
 * parameters when they are not set. For example when desired limits are
 * not specified in PtlNIInit.
 *
 * @param[in] the index of the parameter.
 *
 * @return the current value.
 */
static inline long get_param(int parm)
{
	return param[parm].val;
}

#endif /* PTL_PARAM_H */

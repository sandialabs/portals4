#ifndef PTL_TEST_H
#define PTS_TEST_H

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <portals4.h>
#include <support/support.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dict.h"
#include "api.h"

extern int verbose;
extern int debug;

typedef union datatype {
	int8_t		s8;
	uint8_t		u8;
	int16_t		s16;
	uint16_t	u16;
	int32_t		s32;
	uint32_t	u32;
	int64_t		s64;
	uint64_t	u64;
	float		f;
	float		fc[2];
	double		d;
	double		dc[2];
} datatype_t;

#define PTL_TEST_VERSION	("0.1.0")
#define PTL_TEST_RETURN_OK	(0)
#define PTL_TEST_RETURN_HELP	(1)
#define PTL_TEST_RETURN_EXIT	(2)

#define MAP_SIZE		(10)
#define IOV_SIZE			(32)	/* number of iovecs in an MD/ME/LE */
#define IOVEC_LENGTH		(1024)	/* size of an iovec */
#define EQ_LIST_SIZE		(10)
#define CT_LIST_SIZE		(10)
#define STACK_SIZE		(20)

struct node_info;

struct thread_info {
	struct node_info	*info;
	pthread_t		thread;
	int			num;
	int			run;
	int			errs;
	xmlNode			*node;
};

/*
 * every parameter used in API
 */
struct node_info {
	struct node_info	*next;
	struct node_info	*prev;
	int			ret;
	int			err;
	void			*ptr;
	int			count;
	int			cond;
	volatile struct thread_info	*threads;
	int			thread_id;

	/*
	 * ni_init, ni_fini, ni_status, ni_handle
	 */
	ptl_interface_t		iface;
	unsigned int		ni_opt;
	ptl_rank_t		rank;
	ptl_pid_t		pid;
	ptl_ni_limits_t		desired;
	ptl_ni_limits_t		*desired_ptr;
	ptl_ni_limits_t		actual;
	ptl_ni_limits_t		*actual_ptr;
	ptl_handle_ni_t		ni_handle;
	ptl_sr_index_t		reg;
	ptl_sr_value_t		status;
	ptl_handle_any_t	handle;
	
	/*
	 * pt_alloc, pt_free, pt_disable, pt_enable
	 */
	unsigned int		pt_opt;
	ptl_pt_index_t		pt_index;

	/*
 	 * get_uid, get_id
	 */
	ptl_uid_t		uid;
	ptl_process_t		id;

	/*
	 */
	ptl_iovec_t		iov[IOV_SIZE];

	/*
	 * Node buffer - can be buffer for MD/LE/ME.  A non-zero
	 * flag indicates this node allocated buffer, otherwise node has
	 * access to the previous nodes buffer from shallow copy.
	 */
	unsigned int		buf_alloc;
	unsigned char		*buf;

	/*
	 * md_bind, md_release
	 */
	ptl_md_t		md;
	ptl_handle_md_t		md_handle;
	datatype_t		md_data;

	/*
	 * le_append, le_unlink, le_search
	 */
	ptl_le_t		le;
	ptl_handle_le_t		le_handle;
	ptl_list_t		list;
	datatype_t		le_data;
	ptl_search_op_t		search_op;

	/*
	 * me_append, me_unlink, me_search
	 */
	ptl_me_t		me;
	ptl_handle_me_t		me_handle;
	datatype_t		me_data;

	/*
	 * eq_alloc, eq_free, eq_get, eq_wait, eq_poll
	 */
	ptl_handle_eq_t		eq_handle;
	ptl_event_t		eq_event;
	int			eq_count;
	ptl_event_t		*eq_list[EQ_LIST_SIZE];
	int			eq_size;
	ptl_time_t		timeout;
	unsigned int	which;
	unsigned int	*which_ptr;

	/*
	 * ct_alloc, ct_free, ct_get, ct_wait
	 */
	ptl_handle_ct_t		ct_handle;
	ptl_ct_event_t		ct_event;
	ptl_event_t		*ct_list[CT_LIST_SIZE];
	int			ct_size;
	ptl_size_t		ct_test;

	/*
	 * put, get, atomic, fetch_atomic, swap
	 * triggered put, ...
	 */
	ptl_size_t		loc_offset;
	ptl_size_t		length;
	ptl_ack_req_t           ack_req;
	ptl_process_t		target_id;
	ptl_match_bits_t	match;
	ptl_size_t		rem_offset;
	void			*user_ptr;
	ptl_hdr_data_t		hdr_data;
	ptl_op_t		atom_op;
	ptl_datatype_t		type;
	ptl_handle_md_t		put_md_handle;
	ptl_handle_md_t		get_md_handle;
	ptl_size_t		loc_get_offset;
	ptl_size_t		loc_put_offset;
	ptl_size_t		threshold;
	ptl_handle_ct_t		trig_ct_handle;
	datatype_t		operand;

	/*
	 * handle_is_equal
	 */
	ptl_handle_any_t	handle1;
	ptl_handle_any_t	handle2;

	/*
	 * get/set map
	 */
	ptl_size_t		map_size;
	ptl_process_t  *desired_map_ptr;

	ptl_size_t		get_map_size;
	ptl_size_t      actual_map_size;
	ptl_process_t  *mapping;

	/*
	 * object stacks
	 */
	ptl_handle_ni_t		ni_stack[STACK_SIZE];
	int			next_ni;
	ptl_pt_index_t		pt_stack[STACK_SIZE];
	int			next_pt;
	ptl_handle_md_t		md_stack[STACK_SIZE];
	int			next_md;
	ptl_handle_le_t		le_stack[STACK_SIZE];
	int			next_le;
	ptl_handle_me_t		me_stack[STACK_SIZE];
	int			next_me;
	ptl_handle_eq_t		eq_stack[STACK_SIZE];
	int			next_eq;
	ptl_handle_ct_t		ct_stack[STACK_SIZE];
	int			next_ct;
};

/* enum.c */
int get_ret(char *val);
int get_atom_op(char *val);
int get_atom_type(char *val);
int get_list(char *val);
int get_search_op(char *val);
int get_ack_req(char *val);
int get_event_type(char *val);
int get_ni_fail(char *val);

/* mask.c */
unsigned int get_ni_opt(char *val);
unsigned int get_ni_features(char *val);
unsigned int get_pt_opt(char *val);
unsigned int get_md_opt(char *val);
unsigned int get_me_opt(char *val);
unsigned int get_le_opt(char *val);

/* run.c */
int run_doc(xmlDocPtr doc);

/* rt.c */
int ompi_rt_init(struct node_info *info);
int ompi_rt_fini(struct node_info *info);

#endif /* PTL_TEST_H */

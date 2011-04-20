#ifndef PTL_LOC_H
#define PTL_LOC_H

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <infiniband/verbs.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <rdma/rdma_cma.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <sys/time.h>
#include <search.h>

struct timeval start_time;
struct timeval stop_time;
int test_time;

#include "portals4.h"

/* branch prediction hints for compiler */
#define unlikely(x)	__builtin_expect((x),0)
#define likely(x)	__builtin_expect((x),1)

/* use these for network byte order */
typedef uint16_t	__be16;
typedef uint32_t	__be32;
typedef uint64_t	__be64;

unsigned int pagesize;
unsigned int linesize;

#include "ptl_log.h"
#include "ptl_types.h"
#include "ptl_param.h"
#include "ptl_ref.h"
#include "ptl_evloop.h"
#include "ptl_index.h"
#include "ptl_obj.h"
#include "ptl_gbl.h"
#include "ptl_pt.h"
#include "ptl_rt.h"
#include "ptl_conn.h"
#include "ptl_ni.h"
#include "ptl_mr.h"
#include "ptl_md.h"
#include "ptl_le.h"
#include "ptl_me.h"
#include "ptl_xx.h"
#include "ptl_buf.h"
#include "ptl_eq.h"
#include "ptl_ct.h"
#include "ptl_data.h"
#include "ptl_hdr.h"

extern int debug;
extern int atom_type_size[];

#define WARN()	do { if (debug) printf("\033[1;33mwarn:\033[0m %s(%s:%d)\n", __func__, __FILE__, __LINE__); } while(0)

#define PTL_NI_PORT	(0x4567)
unsigned short ptl_ni_port(ni_t *ni);

#ifndef cpu_to_be16
#define cpu_to_be16(x)	htons(x)
#endif

#ifndef be16_to_cpu
#define be16_to_cpu(x)	htons(x)
#endif

#ifndef cpu_to_be32
#define cpu_to_be32(x)	htonl(x)
#endif

#ifndef be32_to_cpu
#define be32_to_cpu(x)	htonl(x)
#endif

#ifndef cpu_to_be64
/* TODO make this be the builtin fcn */
static inline uint64_t cpu_to_be64(uint64_t x)
{
	uint64_t y = htonl((uint32_t)x);

	return (y << 32) | htonl((uint32_t)(x >> 32));
}
#endif

#ifndef be64_to_cpu
/* TODO make this be the builtin fcn */
static inline uint64_t be64_to_cpu(uint64_t x)
{
	return cpu_to_be64(x);
}
#endif

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
	double		d;
} datatype_t;

typedef int (*atom_op_t)(void *src, void *dst, ptl_size_t length);
extern atom_op_t atom_op[PTL_OP_LAST][PTL_DATATYPE_LAST];

enum {
	STATE_RECV_EVENT_WAIT,
	STATE_RECV_COMP_REARM,
	STATE_RECV_COMP_POLL,
	STATE_RECV_GET_BUF,
	STATE_RECV_WAIT,
	STATE_RECV_SEND_COMP,
	STATE_RECV_RDMA_COMP,
	STATE_RECV_PACKET,
	STATE_RECV_DROP_BUF,
	STATE_RECV_REQ,
	STATE_RECV_TGT,
	STATE_RECV_INIT,
	STATE_RECV_ERROR,
	STATE_RECV_DONE,
};

enum {
	STATE_TGT_START,
	STATE_TGT_DROP,
	STATE_TGT_GET_MATCH,
	STATE_TGT_GET_PERM,
	STATE_TGT_GET_LENGTH,
	STATE_TGT_WAIT_CONN,
	STATE_TGT_DATA_IN,
	STATE_TGT_RDMA,
	STATE_TGT_ATOMIC_DATA_IN,
	STATE_TGT_SWAP_DATA_IN,
	STATE_TGT_DATA_OUT,
	STATE_TGT_RDMA_DESC,
	STATE_TGT_RDMA_WAIT_DESC,
	STATE_TGT_SHORT_DATA_IN,
	STATE_TGT_SHORT_DATA_OUT,
	STATE_TGT_UNLINK,
	STATE_TGT_SEND_ACK,
	STATE_TGT_SEND_REPLY,
	STATE_TGT_COMM_EVENT,
	STATE_TGT_CLEANUP,
	STATE_TGT_ERROR,
	STATE_TGT_DONE,
};

enum {
	STATE_INIT_START,
	STATE_INIT_WAIT_CONN,
	STATE_INIT_SEND_REQ,
	STATE_INIT_SEND_ERROR,
	STATE_INIT_WAIT_COMP,
	STATE_INIT_HANDLE_COMP,
	STATE_INIT_EARLY_SEND_EVENT,
	STATE_INIT_GET_RECV,
	STATE_INIT_WAIT_RECV,
	STATE_INIT_HANDLE_RECV,
	STATE_INIT_LATE_SEND_EVENT,
	STATE_INIT_ACK_EVENT,
	STATE_INIT_REPLY_EVENT,
	STATE_INIT_CLEANUP,
	STATE_INIT_ERROR,
	STATE_INIT_DONE,
	STATE_INIT_LAST,
};

/* RDMA CM private data */
struct cm_priv_request {
	uint32_t options;	  /* NI options (physical/logical, ...) */
	// TODO: make network safe
	ptl_process_t src_id;		/* rank or NID/PID requesting that connection */
};

#define REJECT_REASON_NO_NI			1 /* NI options don't match */
#define REJECT_REASON_GOOD_SRQ		2 /* no main process, SRQ # is good */
#define REJECT_REASON_BAD_PARAM		3 /* request parm is invalid */
#define REJECT_REASON_CONNECTED		4 /* already connected */
#define REJECT_REASON_ERROR			5 /* something unexpected happened; catch all */
struct cm_priv_reject {
	uint32_t reason;
	uint32_t xrc_srq_num;
};

struct cm_priv_accept {
	uint32_t xrc_srq_num;
};

/* In current implementation a NID is just an IPv4 address in host order. */
static inline in_addr_t nid_to_addr(ptl_nid_t nid)
{
	return htonl(nid);
}

static inline ptl_nid_t addr_to_nid(struct sockaddr_in *sin)
{
	return ntohl(sin->sin_addr.s_addr);
}

/* A PID is a port in host order. */
static inline __be16 pid_to_port(ptl_pid_t pid)
{
	if (pid == PTL_PID_ANY) {
		return 0;
	} else {
		return htons(pid);
	}
}

static inline ptl_pid_t port_to_pid(__be16 port)
{
	return ntohs(port);
}

void session_list_is_empty(void);

int send_message(buf_t *buf);

int iov_copy_in(void *src, ptl_iovec_t *iov, ptl_size_t num_iov,
		ptl_size_t offset, ptl_size_t length);

int iov_copy_out(void *dst, ptl_iovec_t *iov, ptl_size_t num_iov,
		 ptl_size_t offset, ptl_size_t length);

int iov_atomic_in(atom_op_t op, void *src, ptl_iovec_t *iov,
		  ptl_size_t num_iov, ptl_size_t offset, ptl_size_t length);

int rdma_read(buf_t *rdma_buf, uint64_t raddr, uint32_t rkey,
	      struct ibv_sge *loc_sge, int num_loc_sge, uint8_t comp);

int post_tgt_rdma(xt_t *xt, data_dir_t dir);

void process_recv(EV_P_ ev_io *w, int revents);

int process_init(xi_t *xi);

int process_tgt(xt_t *xt);

#endif /* PTL_LOC_H */

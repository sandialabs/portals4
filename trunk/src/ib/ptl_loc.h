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
#include <sys/ioctl.h>
#include <endian.h>

#include "tree.h"

#include "config.h"

#include "portals4.h"

#ifdef NO_ARG_VALIDATION
static const int check_param = 0;
#else
static const int check_param = 1;
#endif

/* branch prediction hints for compiler */
#define unlikely(x)	__builtin_expect((x),0)
#define likely(x)	__builtin_expect((x),1)

/* use these for network byte order */
typedef uint16_t	__be16;
typedef uint32_t	__be32;
typedef uint64_t	__be64;
typedef uint16_t	__le16;
typedef uint32_t	__le32;
typedef uint64_t	__le64;

extern unsigned int pagesize;
extern unsigned int linesize;

#include "ptl_log.h"
#include "ptl_types.h"
#include "ptl_atomic.h"
#include "ptl_param.h"
#include "ptl_ref.h"
#include "ptl_evloop.h"
#include "ptl_obj.h"
#include "ptl_iface.h"
#include "ptl_gbl.h"
#include "ptl_pt.h"
#include "ptl_conn.h"
#include "ptl_ni.h"
#include "ptl_mr.h"
#include "ptl_md.h"
#include "ptl_le.h"
#include "ptl_me.h"
#include "ptl_buf.h"
#include "ptl_eq.h"
#include "ptl_ct.h"
#include "ptl_data.h"
#include "ptl_hdr.h"

/* SHMEM. */
#include "ptl_internal_assert.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_alignment.h"
#include "ptl_nemesis.h"

extern int debug;

#define WARN()	do { if (debug) printf("\033[1;33mwarn:\033[0m %s(%s:%d)\n", __func__, __FILE__, __LINE__); } while(0)

static inline __be16 cpu_to_be16(uint16_t x) { return htons(x); }
static inline uint16_t be16_to_cpu(__be16 x) { return htons(x); }
static inline __be32 cpu_to_be32(uint32_t x) { return htonl(x); }
static inline uint32_t be32_to_cpu(__be32 x) { return htonl(x); }
static inline __be64 cpu_to_be64(uint64_t x) {
	uint64_t y = htonl((uint32_t)x);
	return (y << 32) | htonl((uint32_t)(x >> 32));
}
static inline uint64_t be64_to_cpu(__be64 x) { return cpu_to_be64(x); }

#if __BYTE_ORDER==__LITTLE_ENDIAN
static inline __le16 cpu_to_le16(uint16_t x) { return x; }
static inline uint16_t le16_to_cpu(__le16 x) { return x; }
static inline __le32 cpu_to_le32(uint32_t x) { return x; }
static inline uint32_t le32_to_cpu(__le32 x) { return x; }
static inline __le64 cpu_to_le64(uint64_t x) { return x; }
static inline uint64_t le64_to_cpu(__le64 x) { return x; }
#else
#error Not defined yet.
#endif

enum recv_state {
	STATE_RECV_SEND_COMP,
	STATE_RECV_RDMA_COMP,
	STATE_RECV_PACKET_RDMA,
	STATE_RECV_PACKET,
	STATE_RECV_DROP_BUF,
	STATE_RECV_REQ,
	STATE_RECV_INIT,
	STATE_RECV_REPOST,
	STATE_RECV_ERROR,
	STATE_RECV_DONE,
};

enum tgt_state {
	STATE_TGT_START,
	STATE_TGT_DROP,
	STATE_TGT_GET_MATCH,
	STATE_TGT_GET_LENGTH,
	STATE_TGT_WAIT_CONN,
	STATE_TGT_DATA_IN,
	STATE_TGT_RDMA,
	STATE_TGT_ATOMIC_DATA_IN,
	STATE_TGT_SWAP_DATA_IN,
	STATE_TGT_DATA_OUT,
	STATE_TGT_RDMA_DESC,
	STATE_TGT_RDMA_WAIT_DESC,
	STATE_TGT_SHMEM_DESC,
	STATE_TGT_SEND_ACK,
	STATE_TGT_SEND_REPLY,
	STATE_TGT_COMM_EVENT,
	STATE_TGT_WAIT_APPEND,
	STATE_TGT_OVERFLOW_EVENT,
	STATE_TGT_CLEANUP,
	STATE_TGT_CLEANUP_2,
	STATE_TGT_ERROR,
	STATE_TGT_DONE,
};

enum init_state {
	STATE_INIT_START,
	STATE_INIT_PREP_REQ,
	STATE_INIT_WAIT_CONN,
	STATE_INIT_SEND_REQ,
	STATE_INIT_WAIT_COMP,
	STATE_INIT_SEND_ERROR,
	STATE_INIT_EARLY_SEND_EVENT,
	STATE_INIT_WAIT_RECV,
	STATE_INIT_DATA_IN,
	STATE_INIT_LATE_SEND_EVENT,
	STATE_INIT_ACK_EVENT,
	STATE_INIT_REPLY_EVENT,
	STATE_INIT_CLEANUP,
	STATE_INIT_ERROR,
	STATE_INIT_DONE,
	STATE_INIT_LAST,
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

static inline void SPINLOCK_BODY(void)
{
	__asm__ __volatile__ ("pause" ::: "memory");
}

int send_message_rdma(buf_t *buf, int signaled);

int iov_copy_out(void *dst, ptl_iovec_t *iov, ptl_size_t num_iov,
		 ptl_size_t offset, ptl_size_t length);

int iov_copy_in(void *src, ptl_iovec_t *iov, ptl_size_t num_iov,
		ptl_size_t offset, ptl_size_t length, void **dst_start);

int iov_atomic_in(atom_op_t op, int atom_size, void *src, ptl_iovec_t *iov,
		  ptl_size_t num_iov, ptl_size_t offset, ptl_size_t length);

ptl_size_t iov_count_elem(ptl_iovec_t *iov, ptl_size_t num_iov,
			  ptl_size_t offset, ptl_size_t length,
			  ptl_size_t *index_p, ptl_size_t *base_p);

int swap_data_in(ptl_op_t atom_op, ptl_datatype_t atom_type,
		 void *dest, void *source, void *operand);

int rdma_read(buf_t *buf, buf_t *rdma_buf, uint64_t raddr, uint32_t rkey,
	      struct ibv_sge *loc_sge, int num_loc_sge, uint8_t comp);

int post_tgt_rdma(buf_t *buf);

void *process_recv_rdma_thread(void *arg);
void process_recv_shmem(ni_t *ni, buf_t *buf);

int process_init(buf_t *buf);

int process_tgt(buf_t *buf);

int check_overflow(le_t *le);
int check_overflow_search_only(le_t *le);
int check_overflow_search_delete(le_t *le);

buf_t *tgt_alloc_rdma_buf(buf_t *buf);

#ifdef WITH_TRANSPORT_SHMEM
int knem_init(ni_t *ni);
void knem_fini(ni_t *ni);
uint64_t knem_register(ni_t *ni, void *data, ptl_size_t len, int prot);
void knem_unregister(ni_t *ni, uint64_t cookie);
size_t knem_copy_from(ni_t * ni, void *dst,
					  uint64_t cookie, uint64_t off, size_t len);
size_t knem_copy_to(ni_t * ni, uint64_t cookie,
					uint64_t off, void *src, size_t len);
size_t knem_copy(ni_t * ni,
				 uint64_t scookie, uint64_t soffset, 
				 uint64_t dcookie, uint64_t doffset,
				 size_t length);
extern int PtlNIInit_shmem(iface_t *iface, ni_t *ni);
extern int PtlNIInit_shmem_part2(ni_t *ni);
void cleanup_shmem(ni_t *ni);
#else
static inline uint64_t knem_register(ni_t *ni, void *data, ptl_size_t len, int prot)
{
	return 1;
}
static inline void knem_unregister(ni_t *ni, uint64_t cookie) { }
static inline int PtlNIInit_shmem(iface_t *iface, ni_t *ni) { return PTL_OK; }
static inline int PtlNIInit_shmem_part2(ni_t *ni) { return PTL_OK; }
static inline void cleanup_shmem(ni_t *ni) { }

#endif

void PtlInitParam(void);

/* For the runtime. */
extern int ptl_log_level;

#endif /* PTL_LOC_H */

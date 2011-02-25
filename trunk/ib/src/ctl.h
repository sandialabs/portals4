#ifndef CTL_H
#define CTL_H

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <infiniband/verbs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <rdma/rdma_cma.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "portals4.h"

#include "ptl_types.h"
#include "ptl_log.h"
#include "ptl_rpc.h"
#include "ptl_shared.h"

/* branch prediction hints for compiler */
#define unlikely(x)	__builtin_expect((x),0)
#define likely(x)	__builtin_expect((x),1)

struct ib_intf {
	struct list_head list;
	char name[IF_NAMESIZE];

	struct ibv_context *ibv_context;
	int xrc_domain_fd;
	struct ibv_xrc_domain *xrc_domain;
	char xrc_domain_fname[1024];
	struct rdma_cm_id *listen_id;
};

/* network interface (ie. ib0) */
struct net_intf {
	struct list_head list;
	char name[IF_NAMESIZE];
	int index;	/* interface index, returned by if_nameindex() */
	struct ib_intf *ib_intf;
};

extern int load_rank_table(const char *name);
extern int create_ib_resources(void);
extern void destroy_ib_resources(void);
extern struct net_intf *find_net_intf(const char *name);
extern int create_shared_memory(void);

/* Port on which RDMA listen wait for incoming XRC connection. */
#define XRC_PORT 7694

struct p4oibd_config {
	/* Listening socket */
	unsigned int ctl_port;
	struct rpc *rpc;

	/* From configuration files */
	struct rank_table *local_rank_table;  /* local, not shared */
	struct rank_table *master_rank_table; /* in shared memory */

	unsigned int local_nranks;	/* number of ranks on that node */
	unsigned int nranks;		/* total number of rank for job */

	ptl_jid_t jobid;
	ptl_nid_t nid;				/* Local NID */
	ptl_nid_t master_nid;		/* NID of master control */

	/* Session from ranks. One per local rank. */
	struct session **sessions;
	int num_sessions;

	unsigned int recv_nranks;	/* number of rank waiting. When this
								   number is nranks, then the rank
								   table is complete. */

	/* Shared memory. */
	struct {
		char filename[1024];
		size_t filesize;
		int fd;
		struct shared_config *m; /* mmaped memory */
	} shmem;
};

extern struct p4oibd_config conf;

/* RDMA CM private data */
struct cm_priv_request {
	uint32_t src_rank;	/* rank requesting that connection */
};

struct cm_priv_response {
};

#endif /* CTL_H */

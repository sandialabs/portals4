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
#include <poll.h>

#include <ev.h>

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

struct ctl_connect {
	enum {
		PTL_CONNECT_DISCONNECTED,
		PTL_CONNECT_CONNECTING,
		PTL_CONNECT_CONNECTED,
		PTL_CONNECT_DISCONNECTING,
	} state;
	struct rdma_cm_id *cm_id;
};

struct p4oibd_config {
	pthread_mutex_t		mutex;

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
	unsigned int num_nids;		/* number of nodes */

	/* Session count, from ranks. */
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

	/* IB */
	struct list_head net_interfaces;	
	struct list_head ib_interfaces;	
	struct rdma_event_channel *cm_channel;
	ev_io cm_watcher;

	/* XRC port to listen to */
	int xrc_port;

	/* Connection table from remote rank to control process.
	 * nranks elements, protected by mutex. */
	struct ctl_connect *connect;
};

#define MAX_QP_RECV_WR		(10)

extern int load_rank_table(const char *name);
extern int create_ib_resources(struct p4oibd_config *conf);
extern void destroy_ib_resources(struct p4oibd_config *conf);
extern struct net_intf *find_net_intf(struct p4oibd_config *conf, const char *name);
extern int create_shared_memory(struct p4oibd_config *conf);

#endif /* CTL_H */

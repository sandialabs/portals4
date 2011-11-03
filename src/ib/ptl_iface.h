/**
 * @file ptl_iface.h
 *
 * @brief Declarations for ptl_iface.c.
 */

#ifndef PTL_IFACE_H
#define PTL_IFACE_H

/* forward declaration */
struct gbl;

/** @brief Size of ni table per iface */
#define MAX_NI_TYPES		(4)	

/**
 * @brief Per network interface information.
 */
struct iface {
	/** The portals iface_id assigned to this interface */
	ptl_interface_t		iface_id;
	/** Table of NI's indexed by ni type */
	struct ni		*ni[MAX_NI_TYPES];
	/** Network interface name for debugging output */
	char			ifname[IF_NAMESIZE];
	/** The NID/PID for this interface */
	ptl_process_t		id;
	/** The CM event channel for this interface */
	struct rdma_event_channel *cm_channel;
	/** The CM ID for this interface */
	struct rdma_cm_id	*listen_id;
	/** Boolean is true if interface is listening for connections */
	int			listen;
	/** IPV4 address if this interface */
	struct sockaddr_in	sin;
	/** RDMA device info for this interface */
	struct ibv_context	*ibv_context;
	/** RDMA protection domain for this interface */
	struct ibv_pd		*pd;
	/** Libev handler for CM events */
	ev_io			cm_watcher;
};

typedef struct iface iface_t;

void cleanup_iface(iface_t *iface);

int init_iface(iface_t *iface);

int init_iface_table(struct gbl *gbl);

void iface_fini(struct gbl *gbl);

iface_t *get_iface(struct gbl *gbl, ptl_interface_t iface_id);

struct ni *__iface_get_ni(iface_t *iface, int ni_type);

void __iface_add_ni(iface_t *iface, struct ni *ni);

void __iface_remove_ni(struct ni *ni);

int __iface_bind(iface_t *iface, unsigned int port);

#endif /* PTL_IFACE_H */

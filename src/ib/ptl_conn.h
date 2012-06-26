/**
 * @file ptl_conn.h
 *
 * This file contains declarations for ptl_conn.c
 * (See ptl_conn.c for detailed comments.)
 */

#ifndef PTL_CONN_H
#define PTL_CONN_H

#include "ptl_locks.h"

/* forward declarations */
struct ni;
struct buf;

enum conn_state {
	CONN_STATE_DISCONNECTED,
	CONN_STATE_DISCONNECTING,
	CONN_STATE_RESOLVING_ADDR,
	CONN_STATE_RESOLVING_ROUTE,
	CONN_STATE_CONNECTING,
	CONN_STATE_CONNECTED,
};

enum transport_type {
#if WITH_TRANSPORT_IB
	CONN_TYPE_RDMA = 1,
#endif
#if WITH_TRANSPORT_SHMEM
	CONN_TYPE_SHMEM,
#endif
#if WITH_PPE
	CONN_TYPE_MEM,
#endif
};

struct md;
struct data;

/**
 * Per transport methods.
 */
struct transport {
	enum transport_type	type;

	/* Allocate a transport buffer suited for this transport. */
	int (*buf_alloc)(ni_t *ni, struct buf **buf_p);

	/* Sends a short or long message. */
	int (*send_message)(struct buf *buf, int from_init);
	int (*post_tgt_dma)(struct buf *buf);

	/* Sets some sent flags, which determine what to do once the
	 * buffer has been sent. So far, it's either XX_INLINE or
	 * XX_SIGNALED. can_signal is a bit hacking, and indicates to IB
	 * whether it can signal the completion or not, if it can't
	 * inline. can_signal is ignored on other transports. */
	void (*set_send_flags)(struct buf *buf, int can_signal);

	/* Prepare the initiator to transfer the data. It can copy the
	 * immediate data into the buffer if it fits, or prepare some
	 * descriptor to transfer that data with other means (RDMA,
	 * KNEM, ...). */
	int (*init_prepare_transfer)(struct md *md, data_dir_t dir, ptl_size_t offset,
								 ptl_size_t length, struct buf *buf);

	/* Prepare to copy the data in/out in the target. Similar to
	 * append_init_data on the initiator. */
	int (*tgt_data_out)(struct buf *buf, struct data *data);
};

extern struct transport transport_rdma;
extern struct transport transport_shmem;

/**
 * Per connection information.
 */
struct conn {
	/** base object */
	obj_t			obj;

	ptl_process_t		id;		/* dest nid/pid keep first */
	pthread_mutex_t		mutex;
	enum conn_state		state;
	struct sockaddr_in	sin;
	struct list_head	buf_list;
	PTL_FASTLOCK_TYPE	wait_list_lock;

	struct transport	transport;

	union {
#if WITH_TRANSPORT_IB
		struct {
			struct rdma_cm_id	*cm_id;
			int			retry_resolve_addr;
			int			retry_resolve_route;
			int			retry_connect;

			int max_inline_data;

			/* If no completion has been requested in a while, we need
			 * to ask for one. If none is ever requested, then it's
			 * possible the send queue will become full because none
			 * of the sends are flushed. The completion generated will
			 * be ignored by the receiver. */
			atomic_t send_comp_threshold;
			atomic_t rdma_comp_threshold;

			/* Count the number of posted work requests and the number
			 * of work requests since last time a completion was
			 * requested. This is used to rate limit the initiator. */
			atomic_t num_req_posted;
			atomic_t num_req_not_comp;
			
			/* Limit on the number of outstanding requests send
			 * buffers (ie. from the initiator). Set once after the
			 * connection is established. There's an implicit
			 * requirement that every process in the cluster is
			 * running with the same values.
			 * TODO: negociate values during connection setup. */
			int max_req_avail;

			/* local_disc is set to 1 when the local side is ready to
			 * shutdown and has sent its in band disconnect request,
			 * and 2 when that send request has completed, meaning all
			 * the sends have been flushed.
			 * 
			 * remote_disc is set to 1 when the remote in band
			 * disconnect request is received.
			 *
			 * We can truly disconnect only when local_disc == 2 and
			 * remote_disc == 1. */
			int local_disc;
			int remote_disc;

		} rdma;
#endif

#if WITH_TRANSPORT_SHMEM
		struct {
			ptl_rank_t	local_rank;	/* local rank on that node. */
		} shmem;
#endif
	};
};

typedef struct conn conn_t;

/**
 * Allocate a connection from the connect pool.
 *
 * @param ni from which to allocate the conn
 * @param conn_p pointer to return value
 *
 * @return status
 */
static inline int conn_alloc(struct ni *ni, conn_t **conn_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->conn_pool, &obj);
	if (err) {
		*conn_p = NULL;
		return err;
	}

	*conn_p = container_of(obj, conn_t, obj);

	return PTL_OK;
}

/**
 * Take a reference to a conn.
 *
 * @param conn on which to take a reference
 */
static inline void conn_get(conn_t *conn)
{
	obj_get(&conn->obj);
}

/**
 * Drop a reference to a conn
 *
 * If the last reference has been dropped the conn
 * will be freed.
 *
 * @param conn on which to drop a reference
 *
 * @return status
 */
static inline int conn_put(conn_t *conn)
{
	return obj_put(&conn->obj);
}

/* Wait for all to be disconnected. RDMA CM is handling disconnection
 * timeouts, so we should never block forever because of this test. */
static inline int is_disconnected_all(ni_t *ni)
{
	return atomic_read(&ni->rdma.num_conn) == 0;
}

/* RDMA CM private data */
struct cm_priv_request {
	uint32_t		options;	  /* NI options (physical/logical, ...) */
	// TODO: make network safe
	ptl_process_t		src_id;		/* rank or NID/PID requesting that connection */
};

enum {
	REJECT_REASON_NO_NI	= 1, /* NI options don't match */
	REJECT_REASON_GOOD_SRQ,		/* no main process, SRQ # is good */
	REJECT_REASON_BAD_PARAM,	/* request parm is invalid */
	REJECT_REASON_CONNECTED,	/* already connected */
	REJECT_REASON_CONNECTING,	/* already connected */
	REJECT_REASON_ERROR, /* something unexpected happened; catch all */
};

struct cm_priv_reject {
	uint32_t		reason;
};

struct cm_priv_accept {
};

conn_t *get_conn(struct ni *ni, ptl_process_t id);

void destroy_conns(struct ni *ni);

int conn_init(void *arg, void *parm);

void conn_fini(void *arg);

int init_connect(struct ni *ni, conn_t *conn);

void process_cm_event(EV_P_ ev_io *w, int revents);

#endif /* PTL_CONN_H */

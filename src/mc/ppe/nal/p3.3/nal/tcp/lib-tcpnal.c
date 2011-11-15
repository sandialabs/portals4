/*
 * This Cplant(TM) source code is part of the Portals3 Reference
 * Implementation.
 *
 * This Cplant(TM) source code is the property of Sandia National
 * Laboratories.
 *
 * This Cplant(TM) source code is copyrighted by Sandia National
 * Laboratories.
 *
 * The redistribution of this Cplant(TM) source code is subject to the
 * terms of version 2 of the GNU General Public License.
 * (See COPYING, or http://www.gnu.org/licenses/lgpl.html.)
 *
 * Cplant(TM) Copyright 1998-2006 Sandia Corporation.
 *
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the US Government.
 * Export of this program may require a license from the United States
 * Government.
 */


/* Portals3 is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License,
 * as published by the Free Software Foundation.
 *
 * Portals3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Portals3; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Questions or comments about this library should be sent to:
 *
 * Jim Schutt
 * Sandia National Laboratories, New Mexico
 * P.O. Box 5800
 * Albuquerque, NM 87185-0806
 *
 * jaschut@sandia.gov
 *
 */


/* This file implements the NAL library side of a Portals 3.3 TCP/IP NAL.
 * The code is designed to support either a kernel-space or completely
 * user-space NAL.  In either case, the NAL is asynchronous and thread-safe.
 * The kernel and user space NALs can interoperate.
 *
 * If PTL_KERNEL_BLD is defined at compile time, the code is compiled for
 * kernel space; otherwise it is compiled for user space.
 *
 * If USER_PROGRESS_THREAD is defined at compile time, a progress POSIX
 * thread is started to move data in/out of sockets.
 */

#if defined PTL_KERNEL_BLD && defined USER_PROGRESS_THREAD
#error PTL_KERNEL_BLD and USER_PROGRESS_THREAD cannot both be defined.
#endif

/* We read these environment variables to tell us about our identity.
 *
 * Required:
 *   PTL_IFACE  - String identifing interface, e.g. myri0, eth1, etc.
 *
 * Optional:
 *   PTL_PID2PORT_FILE - Path to file containing pid -> IP port map for
 *	well-known (fixed) pids.  The file should contain lines with
 *	"<pid> <port>" on each line, for each well-known pid.
 *	File name defaults to ./map_pid2port.
 *   PTL_DEF_PORTLIST - List of TCP ports, colon separated, to seach when
 *	connecting to a pid that is not in the list of well-known pids.
 *
 * Entries for the default port list only need to be specified if the
 * compiled-in values are insufficient/innappropriate for some reason.
 * If any default port values are specified via PTL_PID2PORT_FILE or
 * PTL_DEF_PORTLIST, the compiled-in default portlist is overridden.
 *
 * In this implementation, the nid is the IPV4 address (in host byte
 * order) of the interface  (e.g. myri0, eth1, etc.) used to run portals.
 * We'll start up connections on demand, so we don't waste resources on
 * unused connections.
 *
 * When we start up, if we find our pid in the pid2port list, we bind to
 * that port to listen for new connections. Otherwise, we bind to the
 * first available port in the default port list.
 *
 * When a connection is requested, we look up the target pid in the
 * pid2port list, and try to connect to that port.  If the target pid
 * isn't in the pid2port list, we try to connect to ports in the default
 * port list, in order.  If we connect, we exchange nid,pid information
 * to make sure we've connected to the correct target.
 *
 * Why do we need a default port _list_?  Why isn't a single default port
 * enough?   Because only one process on a given node can be listening on a
 * given port at a time, and we want to handle the case of multiple portals
 * processes on an SMP node without a global pid/port map for ephemeral
 * portals processes (i.e., non-daemon processes).
 *
 * Since we want to have only a single connection between any pair of
 * Portals processes, we get a race if two processes which haven't yet
 * communicated each decide at the same time to send a message to the
 * other.  We resolve the race in favor of the connection initiated by
 * the process with the higher nid, or higher pid in the case of equal nids.
 * Note that one process will have a channel with a send queue which we
 * have to move from the channel we're closing to the channel we're
 * keeping, and _that_ means we can't start processing a send queue on a
 * new channel until we know it won't be closed.
 *
 * Unfortunately, this situation is so racy it takes a three-way handshake
 * to guarantee that we don't send any data on a connection that we'll
 * close later, because we decide it's the duplicate.
 */

#include <p3-config.h>

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#ifndef PTL_KERNEL_BLD
#include <sys/time.h>
#include <sys/types.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#else
#include <linux/ioctl.h>
#include <linux/if.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/inet.h>

#include <kern_errno.h>
#define NEED_P3_DEQUEUE_SIGNAL
#include <kern_syscall.h>
#endif /* PTL_KERNEL_BLD */

#ifdef USER_PROGRESS_THREAD
#include <pthread.h>
#endif

/* If compiling for user-space (and maybe NIC-space), these need to be
 * the Portals3 versions of the Linux kernel include files, which
 * have been suitably modified for use in user-space code.
 */
#include <linux/list.h>

/* These are all Portals3 include files.
 */
#include <p3utils.h>

#include <p3api/types.h>
#include <p3api/debug.h>
#include <p3api/misc.h>

#include <p3/lock.h>
#include <p3/handle.h>
//#include <p3/process.h>
//#include <p3/forward.h>
#include <p3/nal_types.h>
#include <p3/errno.h>
#include <p3/debug.h>

#include <p3lib/types.h>
#include <p3lib/p3lib.h>
#include <p3lib/nal.h>
#include <p3lib/p3lib_support.h>
#include <p3lib/p3validate.h>

#include "lib-tcpnal.h"
#include "hash_int.h"

#ifdef PTL_KERNEL_BLD
#include "p3tcp_module.h"
#endif


/* Note: This NAL has no inherent limits, but we're going to use the
 * minimal compliant value of max_pt_index anyway, just to demonstrate
 * the issues behind arbitrary limits.
 */
static
ptl_ni_limits_t lib_tcp_limits =
{
	//.max_mes = INT_MAX,
	.max_mds = INT_MAX,
	.max_eqs = INT_MAX,
	//.max_ac_index = 31,
	.max_pt_index = 7,
	//.max_md_iovecs = INT_MAX,
	//.max_me_list = INT_MAX,
	//.max_getput_md = INT_MAX
};

typedef struct sockaddr_in sockaddr_in_t;

#define TCP_SOCKBUF_SZ     131072
#define TCP_LISTEN_BACKLOG 32
#define MAX_PORT            ((unsigned long)((1ULL<<8*sizeof(in_port_t)) - 1))

static unsigned p3tcp_def_portlist[] =
{21000, 21001, 21002, 21003, 21004, 21005, 21006, 21007,
 21008, 21009, 21010, 21011, 21012, 21013, 21014, 21015,
 21016, 21017, 21018, 21019, 21020, 21021, 21022, 21023};

static int p3tcp_ndport = sizeof(p3tcp_def_portlist)/sizeof(unsigned);

/* We have a message struct for every outstanding message, since we use
 * non-blocking sockets and we might only partially complete a message
 * on any given attempt on a socket.
 *
 * On receives we have only one in-progress (outstanding) message at a
 * time per channel.  We create a message struct when we read a new
 * header from the socket.  Then we wait until the library calls
 * nal->recv() for that message before we start pulling data out of
 * the socket.
 *
 * On sends we might have multiple messages queued for sending on
 * a channel.
 */
typedef struct p3tcp_chan p3tcp_chan_t;

typedef struct p3tcp_msg {
	lib_ni_t *ni;
	void *libdat;
	void *hdr;
	ptl_md_iovec_t *iov;
	ptl_size_t iovlen;
	ptl_size_t iovos;
	ptl_size_t hdrlen;
	ptl_size_t hdros;
	ptl_size_t datalen;
	ptl_size_t dataos;
	ptl_size_t droplen;
	ptl_size_t dropos;
	struct list_head queue;	/* link into send queue */
#if 0
	struct list_head hash;	/* link into active message hash */
#endif
	p3tcp_chan_t *chan;
	DECLARE_P3_ADDRMAP_HOOK;
} p3tcp_msg_t;


/* LOCKING RULE: you can acquire a channel lock while holding a
 * p3tcp_data lock, but not vice-versa.
 */

/* When this NAL is used in user space, it services a single Portals-using
 * process; when it is used in kernel space, it services all Portals-using
 * processes communicating via a given interface.  Thus, when sending to
 * a kernel-space TCP NAL, we'll have one connection per NID, but when sending
 * to a user-space TCP NAL we'll have one connection per NID/PID pair.
 *
 * We'll handle this by using NID/PTL_PID_ANY as the key for storing channels
 * where the far endpoint is a kernel-space TCP NAL.
 *
 * FIXME:
 * See the README for a discussion of the perils of having an instance of both
 * kernel- and user-space NALs running on the same node.
 */
#ifdef PTL_KERNEL_BLD
#define CHAN_LOCAL_PID(pid) PTL_PID_ANY
#else
#define CHAN_LOCAL_PID(pid) (pid)
#endif
/* When we open a channel, we need to use non-blocking connects, and since
 * we might need to try several destination ports before we find the right
 * one, we need to keep track.  So, use a 16-bit bitfield as an index into
 * the p3tcp_data:defport array for what default port we're trying to
 * connect on, and another bit to track whether we're even trying to use
 * default ports.
 *
 * Also, if we've started trying ports before our target has started
 * listening, the first time we hit the right port it will be closed.
 * So, we'll retry all the ports multiple times, doubling the time we wait
 * after each round.  We give up and signal an error back to the Portals
 * library when we've exhausted our retries with no connection.
 *
 * TCP_RBITS is the number of bits in the retry counter. TCP_RETRY_DELAY
 * is the length of the initial retry delay, in milliseconds.
 */
#define TCP_RBITS           6
#define TCP_CONNECT_RETRIES (1U << (TCP_RBITS-1))
#define TCP_RETRY_DELAY     750

struct p3tcp_chan {
	int fd;

	unsigned defport_idx :16;	/* index into p3tcp_data:defport */
	unsigned use_defports:1;	/* connecting on default ports? */
	unsigned init_sent:1;		/* initial nid/pid handshake send */
	unsigned init_recv:1;		/* initial nid/pid handshake recv'd */
	unsigned conn_init:1;		/* initial nid/pid handshake done */
	unsigned conn_fail:1;		/* connection retry count overflow */
	unsigned conn_retry:TCP_RBITS;	/* connection retry counter */

	timeout_val_t conn_timeout;

	ptl_pid_t lpid;
	ptl_nid_t rnid;
	ptl_pid_t rpid;
	sockaddr_in_t raddr;
	struct list_head fd_hash;	/* find channel via fd */
	struct list_head nid_hash;	/* find channel via remote ptl ids */

	p3tcp_msg_t *recv;
	struct list_head sendq;
	p3lock(sendq_lock);
	int sendq_len;
};

/* We have two channel hash tables, one hashed on remote nid so we can
 * quickly locate which channel to send on, and one based on file
 * descriptor so we can quickly service a channel once we've determined
 * via select() that it is ready to accept/receive more bytes. These
 * macros define the size of the hashes.
 */
#define TCP_FD_HASH_BITS   6
#define TCP_FD_HASH_BINS   (1U<<TCP_FD_HASH_BITS)

#define TCP_NID_HASH_BITS  6
#define TCP_NID_HASH_BINS  (1U<<TCP_NID_HASH_BITS)

typedef struct pid_pt {
	ptl_pid_t pp_pid;
	unsigned pp_port;
} pid_pt_t;


struct p3tcp_data {

	ptl_interface_t type;	/* Our type */
	ptl_nid_t nid;		/* Our NID */

	unsigned up;
	unsigned npids;		/* Length of pid2pt */
	unsigned ndport;	/* Length of defport */
	pid_pt_t *pid2pt;	/* Known (pid,port) pairs, ordered by pid */
	unsigned *defport;	/* List of default ports */

	/* The socket we listen on for new connections
	 */
	int listen_fd;
	sockaddr_in_t laddr;
	char *laddr_str;

	/* Hash tables of list heads for known open channels.
	 */
	struct list_head chan_fd_hash[TCP_FD_HASH_BINS];
	struct list_head chan_nid_hash[TCP_NID_HASH_BINS];

	/* We use these to initialize the fd_sets on each call to select().
	 * when a connection comes up we add its fd to inuse_rd_fds, and
	 * when we close it we take it out.  When we have data to write on
	 * a connection we add its fd to inuse_wr_fds, and when there is no
	 * more data to write on that connection we take it out.
	 *
	 * FIXME: for more than 1024 file descriptors we can use something
	 * like the variably-sized fdsets from PVFS, since at least in linux
	 * there is no hardcoded kernel limit on the number of active fds.
	 */
	int max_fd;		/* largest in-use file descriptor */
	int oconn;		/* number of outstanding connects */
	fd_set inuse_rd_fds;
	fd_set inuse_wr_fds;
	fd_set inuse_co_fds;	/* used to track non-blocking connects */

	p3lock(dlock);

	unsigned int debug;

	/* When we're in kernel space, we need to open connections in the
	 * context of the progress thread, not the application, so use this
	 * list to hold p3tcp_chan_t objects waiting for that.  It doesn't
	 * hurt to do it this way for user space, too.
	 */
	struct list_head conn_wait;

#ifdef PTL_KERNEL_BLD
	struct semaphore poll_sem;
	struct p3tcp_kthread *progress_thread;
#else
	/* In user space, we need to know our PID before we can start up,
	 * but we can't look it up via our p3_process_t since the NI isn't
	 * installed there until the NAL instance has successfully started.
	 * Grrrrrrr.
	 */
	ptl_pid_t pid;

#ifdef USER_PROGRESS_THREAD
	/* File descriptors for the pipe used to wake up the progress thread,
	 * and a wait queue to make sure the progress thread stops before
	 * the API thread .
	 */
	int snd_wakeup;
	int rcv_wakeup;
	struct {
		pthread_mutex_t lock;
		pthread_cond_t wait;
		unsigned flag;
	} shutdown_sync;
#endif
#endif
};

#ifdef PTL_KERNEL_BLD
#define kdown(sem) down((sem))
#define kup(sem) up((sem))
#else
#define kdown(sem) do {} while (0)
#define kup(sem) do {} while (0)
#endif

p3tcp_data_t p3tcp_nal_data[MAX_P3TCP_IF] =
{
#ifdef PTL_KERNEL_BLD
	{PTL_NALTYPE_TCP0, },
	{PTL_NALTYPE_TCP1, },
	{PTL_NALTYPE_TCP2, },
	{PTL_NALTYPE_TCP3, },
#else
	{PTL_NALTYPE_UTCP0, },
	{PTL_NALTYPE_UTCP1, },
	{PTL_NALTYPE_UTCP2, },
	{PTL_NALTYPE_UTCP3, },
#endif
};

int p3tcp_dequeue_msg(lib_ni_t *ni, void *nal_internal_msg_struct);


#ifdef PTL_KERNEL_BLD
void __p3tcp_save_progress_thread(struct p3tcp_data *nal_data,
				  struct p3tcp_kthread *thread)
{
	nal_data->progress_thread = thread;
}
#endif

static inline
int p3tcp_if_idx(ptl_interface_t type)
{
	unsigned if_idx = 0;

	switch (type) {
	case PTL_NALTYPE_TCP3:
	case PTL_NALTYPE_UTCP3:
		if_idx++;
	case PTL_NALTYPE_TCP2:
	case PTL_NALTYPE_UTCP2:
		if_idx++;
	case PTL_NALTYPE_TCP1:
	case PTL_NALTYPE_UTCP1:
		if_idx++;
	}
	return if_idx;
}

p3tcp_data_t *p3tcp_data(ptl_interface_t type)
{
	return p3tcp_nal_data + p3tcp_if_idx(type);
}

/* We use the construct DEBUG_NI(ni, mask) so that debugging generates no
 * code unless debugging is enabled, by always enclosing debugging in an
 * if-block, like this:
 *
 * if (DEBUG_NI(ni, mask_value)) {
 *	 // debugging code
 * }
 *
 * We rely for this on a compiler optimizing away the block of an if-statement
 * that is constant and false at compile-time.
 *
 * Generally, use debugging flags as follows:
 *
 * PTL_DBG_NI_00
 *   Error conditions that should only be reported when debugging is
 *   enabled.
 * PTL_DBG_NI_01
 *   Simplified info regarding connection setup/teardown
 * PTL_DBG_NI_02
 *   Detailed info regarding connection setup/teardown
 * PTL_DBG_NI_03
 *   Very detailed info regarding connection setup/teardown
 * PTL_DBG_NI_04
 *   Simplified info regarding NAL operations
 * PTL_DBG_NI_05
 *   Detailed info regarding NAL operations
 * PTL_DBG_NI_06
 *   Very detailed info regarding NAL operations
 * PTL_DBG_NI_07
 *   _Very_ detailed info regarding NAL operations
 * PTL_DBG_NI_08
 *   Impossibly detailed info regarding NAL operations
 */
#if defined DEBUG_PTL_INTERNALS
#if 0
/* Each level includes those preceeding it
 */
#define DEBUG_NI(obj,dbg) \
	((((obj)->debug & PTL_DBG_NI_ALL) | PTL_DBG_NI_00) >= (dbg))
#else
/* Each level is independent
 */
#define DEBUG_NI(obj,dbg) \
	(((PTL_DBG_NI_ALL & (obj)->debug) && (PTL_DBG_NI_00 & (dbg))) || \
	 ((PTL_DBG_NI_ALL & (dbg) & (obj)->debug) && !(PTL_DBG_NI_00 & (dbg))))
#endif
#else
#define DEBUG_NI(obj,dbg)  0
#endif



static inline void err_abort(int err, char *msg)
{
	p3_print("%s: %s\n", msg, strerror(err));
	p3_flush();
	PTL_ROAD();
}

/* Use this function to get a socket pending error; Stevens says that
 * Solaris has getsockopt return -1 and return the pending error in errno,
 * rather than the standard way, at least for non-blocking connect errors.
 *
 * getsockerr returns 0 if there was no error, or the pending error.
 */
static
inline int getsockerr(int fd)
{
	int err;
	socklen_t errlen = sizeof(err);

	if (getsockopt(fd, SOL_SOCKET,
		       SO_ERROR, (void *)&err, &errlen) < 0) err = errno;
	return err;
}


/* Pass this guy to qsort(3) to order the paired value maps. it works
 * because the C standard says the address of a structure and its
 * first member are the same.  we can also use it for bsearch(3).
 */
static
int map_cmp(const void *v1, const void *v2)
{
	return *(ptl_pid_t *) v1 - *(ptl_pid_t *) v2;
}

static inline
unsigned long p3tcp_msg_id(p3tcp_msg_t *msg)
{
	return (unsigned long)msg;
}

static inline
p3tcp_msg_t *p3tcp_get_msg(p3tcp_data_t *d, unsigned long msg_id)
{
	return (p3tcp_msg_t *)msg_id;
}

static inline
p3tcp_msg_t *p3tcp_new_msg(p3tcp_data_t *d, size_t sz)
{
	p3tcp_msg_t *msg = NULL;

	if (!sz)
		goto out;

	if (!(msg = p3_malloc(sz)))
		goto out;
	memset(msg, 0, sz);
	init_p3_addrmap_hook(msg);
out:
	return msg;
}

static inline
void p3tcp_free_msg(p3tcp_data_t *d, p3tcp_msg_t *msg)
{
	if (!msg)
		return;
	p3_free(msg);
}

/* Call with d->dlock held.
 */
static
void p3tcp_put_chan(p3tcp_data_t *d, p3tcp_chan_t *chan)
{
	ptl_nid_t lnid = d->nid;
	ptl_pid_t lpid = CHAN_LOCAL_PID(chan->lpid);

	p3tcp_chan_t *lc;
	struct list_head *head =
		d->chan_nid_hash + hash32(chan->rnid, TCP_NID_HASH_BITS);
	struct list_head *item = head->next;

	/* To support sends to self, we're going to do the most simple,
	 * stupid host.  So, we need a channel object for each end of the
	 * connection, which means we need to allow a duplicate channel
	 * object for that case.
	 *
	 * A channel whose remote nid/pid are the same as ours is supporting
	 * sends to self.
	 */
	while (item != head) {
		lc = list_entry(item, p3tcp_chan_t, nid_hash);
		if (lc->rnid == chan->rnid) {
			if (lc->rpid == chan->rpid &&
			    (chan->rnid != lnid || chan->rpid != lpid))
				err_abort(EEXIST, "p3tcp_put_chan");
			if (lc->rpid > chan->rpid)
				break;
		}
		if (lc->rnid > chan->rnid)
			break;
		item = item->next;
	}
	list_add_tail(&chan->nid_hash, item);
}

/* Call with d->dlock held.
 */
static
p3tcp_chan_t *p3tcp_get_chan(p3tcp_data_t *d, ptl_nid_t rnid, ptl_pid_t rpid)
{
	p3tcp_chan_t *lc;
	struct list_head *head =
		d->chan_nid_hash + hash32(rnid, TCP_NID_HASH_BITS);
	struct list_head *item = head->next;

	while (item != head) {
		lc = list_entry(item, p3tcp_chan_t, nid_hash);
		if (lc->rnid == rnid) {
			if (lc->rpid == rpid || lc->rpid == PTL_PID_ANY)
				return lc;
			if (lc->rpid > rpid)
				break;
		}
		if (lc->rnid > rnid)
			break;
		item = item->next;
	}
	return NULL;
}

/* Call with d->dlock held.
 */
static
void p3tcp_put_chan_fd(p3tcp_data_t *d, p3tcp_chan_t * chan)
{
	p3tcp_chan_t *lc;
	struct list_head *head =
		d->chan_fd_hash + hash32(chan->fd, TCP_FD_HASH_BITS);
	struct list_head *item = head->next;

	while (item != head) {
		lc = list_entry(item, p3tcp_chan_t, fd_hash);
		if (lc->fd == chan->fd)
			err_abort(EEXIST, "p3tcp_put_chan_fd");
		if (lc->fd > chan->fd)
			break;
		item = item->next;
	}
	list_add_tail(&chan->fd_hash, item);
}

/* Call with d->dlock held.
 */
static
p3tcp_chan_t *p3tcp_get_chan_fd(p3tcp_data_t *d, int fd)
{
	p3tcp_chan_t *lc;
	struct list_head *head =
		d->chan_fd_hash + hash32(fd, TCP_FD_HASH_BITS);
	struct list_head *item = head->next;

	while (item != head) {
		lc = list_entry(item, p3tcp_chan_t, fd_hash);
		if (lc->fd == fd)
			return lc;
		if (lc->fd > fd)
			break;
		item = item->next;
	}
	return NULL;
}

static
int p3tcp_findport(p3tcp_data_t *d, ptl_pid_t pid)
{
	pid_pt_t *pp;

	if (d->npids &&
		(pp = bsearch(&pid, d->pid2pt, d->npids,
			      sizeof(pid_pt_t), map_cmp)))
		return pp->pp_port;
	else
		return -1;
}

/* Call this after a successful connect() or accept() to send some startup
 * data used for consistency checking.  Returns 0 if we were able to send
 * the startup data successfully.
 */
static
int p3tcp_chan_sndinit(p3tcp_data_t *d, p3tcp_chan_t *chan, const char *fn)
{
	int flags, rtn = -1;
	int send_flags = 0;
	ptl_nid_t lnid = d->nid, rnid = chan->rnid;
	ptl_pid_t lpid = chan->lpid, rpid = chan->rpid;

	if (DEBUG_NI(d,PTL_DBG_NI_02)) {
		struct in_addr addr;
		addr.s_addr = htonl(rnid);
		p3_print("%s: try send init chan %p fd %d to %s pid "FMT_PID_T
			 "\n",fn, chan, chan->fd, inet_ntoa(addr), rpid);
	}
	/* Unless the socket buffer is ridiculously small, writing these
	 * values shouldn't be a problem.  So, we'll switch to synchronous I/O
	 * for them, since it would be very inconvenient to use non-blocking
	 * I/O for this and be sure we could handle it if for some reason we
	 * did get EAGAIN.
	 */
	if ((flags = fcntl(chan->fd, F_GETFL, 0)) == -1) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: fcntl fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
		goto out;
	}
	flags &= ~O_NONBLOCK;
	if (fcntl(chan->fd, F_SETFL, flags) == -1) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: fcntl fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
		goto out;
	}

#if HAVE_MSG_NOSIGNAL
	send_flags |= MSG_NOSIGNAL;
#endif

again1:
	if (send(chan->fd, &rnid, sizeof(rnid), send_flags) < 0) {
		if (errno == EINTR || errno == EAGAIN) goto again1;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: send fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
		goto out;
	}
again2:
	if (send(chan->fd, &rpid, sizeof(rpid), send_flags) < 0) {
		if (errno == EINTR || errno == EAGAIN) goto again2;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: send fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
		goto out;
	}
again3:
	if (send(chan->fd, &lnid, sizeof(lnid), send_flags) < 0) {
		if (errno == EINTR || errno == EAGAIN) goto again3;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: send fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
		goto out;
	}
again4:
	if (send(chan->fd, &lpid, sizeof(lpid), send_flags) < 0) {
		if (errno == EINTR || errno == EAGAIN) goto again4;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: send fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
		goto out;
	}

	flags |= O_NONBLOCK;
	if (fcntl(chan->fd, F_SETFL, flags) == -1) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: fcntl: %s\n", fn, strerror(errno));
		goto out;
	}
	if (!rtn && DEBUG_NI(d,PTL_DBG_NI_02)) {
		struct in_addr addr;
		addr.s_addr = htonl(rnid);
		p3_print("%s: sent init chan %p fd %d to %s pid "FMT_PID_T
			 "\n", fn, chan, chan->fd, inet_ntoa(addr), rpid);
	}
	rtn = 0;
out:
	return rtn;
}

/* Call this after a successful connect() or accept() to receive some startup
 * data used for consistency checking.  If initiator == 1 we are receiving
 * startup data on a connection we initiated.
 *
 * Returns zero if the we and the other end of the connection agree that
 * we are each talking to who we think we should be talking to.
 */
static
int p3tcp_chan_rcvinit(p3tcp_data_t *d,
		       p3tcp_chan_t *chan, int initiator, const char *fn)
{
	int rtn = -1;
	ptl_nid_t lnid, rnid;
	ptl_pid_t lpid, rpid;
	ssize_t rc;

	if (DEBUG_NI(d,PTL_DBG_NI_02))
		p3_print("%s: try recv init chan %p fd %d\n",
			 fn, chan, chan->fd);

	/* Blocking reads shouldn't hurt here because we're only
	 * reading a few bytes, which should have easily fit into
	 * the socket buffer.
	 *
	 * If we get zero bytes, someone closed the channel.
	 */
again1:
	rc = recv(chan->fd, &lnid, sizeof(lnid), MSG_WAITALL);
	if (rc < 0) {
		if (errno == EINTR || errno == EAGAIN)
			goto again1;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: recv fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
	}
	if (DEBUG_NI(d,PTL_DBG_NI_07))
		p3_print("%s: fd %d: "FMT_SSZ_T" bytes lnid "FMT_NID_T"\n",
			 fn, chan->fd, rc, lnid);
	if (rc <= 0)
		goto out;
again2:
	rc = recv(chan->fd, &lpid, sizeof(lpid), MSG_WAITALL);
	if (rc < 0) {
		if (errno == EINTR || errno == EAGAIN)
			goto again2;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: recv fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
	}
	if (DEBUG_NI(d,PTL_DBG_NI_07))
		p3_print("%s: fd %d: "FMT_SSZ_T" bytes lpid "FMT_PID_T"\n",
			 fn, chan->fd, rc, lpid);
	if (rc <= 0)
		goto out;
again3:
	rc = recv(chan->fd, &rnid, sizeof(rnid), MSG_WAITALL);
	if (rc < 0) {
		if (errno == EINTR || errno == EAGAIN)
			goto again3;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: recv fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
	}
	if (DEBUG_NI(d,PTL_DBG_NI_07))
		p3_print("%s: fd %d: "FMT_SSZ_T" bytes rnid "FMT_NID_T"\n",
			 fn, chan->fd, rc, rnid);
	if (rc <= 0)
		goto out;
again4:
	rc = recv(chan->fd, &rpid, sizeof(rpid), MSG_WAITALL);
	if (rc < 0) {
		if (errno == EINTR || errno == EAGAIN)
			goto again4;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: recv fd %d: %s\n",
				 fn, chan->fd, strerror(errno));
	}
	if (DEBUG_NI(d,PTL_DBG_NI_07))
		p3_print("%s: fd %d: "FMT_SSZ_T" bytes rpid "FMT_PID_T"\n",
			 fn, chan->fd, rc, rpid);
	if (rc <= 0)
		goto out;

	if ((unsigned) rnid != ntohl(chan->raddr.sin_addr.s_addr)) {
		if (DEBUG_NI(d,PTL_DBG_NI_02))
			p3_print("%s: chan %p invalid remote nid "FMT_NID_T
				 ", should be %"PRIu32"\n", fn, chan, rnid,
				 ntohl(chan->raddr.sin_addr.s_addr));
		goto out;
	}
	if (initiator &&
	    !(rnid == chan->rnid &&
	      (rpid == chan->rpid || rpid == PTL_PID_ANY))) {
		if (DEBUG_NI(d,PTL_DBG_NI_02))
			p3_print("%s: init chan %p remote nid/pid mismatch "
				 "fd %d\n", fn, chan, chan->fd);
		goto out;
	}
	/* If we're in kernel space, a connection handles all PIDs.  So
	 * as long as the NIDs match, we can accept the connection.  The
	 * data for a message to a non-existing PID should get dropped
	 * later.
	 */
	if (!(lnid == d->nid
#ifndef PTL_KERNEL_BLD
	      && p3lib_get_ni_pid(d->type, lpid)
#endif
		    )) {
		if (DEBUG_NI(d,PTL_DBG_NI_02))
			p3_print("%s: init chan %p local nid/pid mismatch "
				 "fd %d\n", fn, chan, chan->fd);
		goto out;
	}
	if (initiator) {
		if (rpid == PTL_PID_ANY)	/* remote is kernel NAL */
			chan->rpid = PTL_PID_ANY;
	}
	else {
		chan->rnid = rnid;
		chan->rpid = rpid;
		chan->lpid = CHAN_LOCAL_PID(lpid);
	}
	if (!rtn && DEBUG_NI(d,PTL_DBG_NI_02)) {
		struct in_addr addr;
		addr.s_addr = htonl(rnid);
		p3_print("%s: recvd init chan %p fd %d from %s pid "FMT_PID_T
			 "\n", fn, chan, chan->fd, inet_ntoa(addr), rpid);
	}
	rtn = 0;
out:
	return rtn;
}

static
void __tcp_close_chan(p3tcp_data_t *d, p3tcp_chan_t *chan)
{
	if (chan->fd >= 0) {
		close(chan->fd);
		FD_CLR(chan->fd, &d->inuse_rd_fds);
		FD_CLR(chan->fd, &d->inuse_wr_fds);
		chan->fd = -1;
	}
}

/* Call with channel sendq lock held; returns with lock dropped
 */
static
void __dequeue_free_msg(const char *fn,
			p3tcp_data_t *d, p3tcp_chan_t *chan, p3tcp_msg_t *msg)
{
	list_del_init(&msg->queue);
	chan->sendq_len--;

	p3_unlock(&chan->sendq_lock);

	p3_addrmap_hook_drop(msg);
	lib_finalize(msg->ni, msg->libdat, PTL_NI_FAIL);

	if (DEBUG_NI(d, PTL_DBG_NI_06))
		p3_print("__dequeue_free_msg(%s): freeing msg %p\n", fn, msg);

	p3tcp_free_msg(d, msg);
}

static
void p3tcp_close_chan(p3tcp_data_t *d, p3tcp_chan_t *chan, unsigned int mask)
{
	static const char *fn = "p3tcp_close_chan";
	p3tcp_msg_t *msg;

	p3_lock(&d->dlock);
	p3_lock(&chan->sendq_lock);

	if (DEBUG_NI(d,mask))
		p3_print("close %s:%d (chan %p fd %d)\n",
			 inet_ntoa(chan->raddr.sin_addr),
			 (int) ntohs(chan->raddr.sin_port), chan, chan->fd);

	__tcp_close_chan(d, chan);

	list_del(&chan->fd_hash);
	list_del(&chan->nid_hash);
	p3_unlock(&chan->sendq_lock);
	p3_unlock(&d->dlock);

	/* We have to finalize with prejudice anything in the send
	 * and receive queues.  We're only called from the progress
	 * thread, so we don't have to worry about deleting messages
	 * that are half-transferred.
	 */
	if (chan->recv) {
		msg = chan->recv;

		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print( "%s: chan %p finalizing in-progress "
				  "receive with prejudice.\n", fn, chan);

		lib_finalize(msg->ni, msg->libdat, PTL_NI_FAIL);
		p3_addrmap_hook_drop(msg);

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: freeing msg %p\n", fn, msg);
		p3tcp_free_msg(d, msg);
	}

	while (!list_empty(&chan->sendq)) {

		msg = list_entry(chan->sendq.prev, p3tcp_msg_t, queue);

		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print( "%s: chan %p finalizing in-progress "
				  "send with prejudice.\n", fn, chan);

		p3_lock(&chan->sendq_lock);
		__dequeue_free_msg(fn, d, chan, msg);
	}
	p3_free(chan);
}

static
int __p3tcp_connect(p3tcp_data_t *d, p3tcp_chan_t *chan, const char *fn)
{
	int flags, sbsz = TCP_SOCKBUF_SZ;
	int s, sz = sizeof(chan->raddr);
#ifdef HAVE_SO_NOSIGPIPE
	int nosigpipe = 1;
#endif

	chan->init_sent = 0;
	chan->init_recv = 0;

	if ((chan->fd = s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: socket: %s\n", fn, strerror(errno));
		goto out_err;
	}
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (void*)&sbsz, sizeof(sbsz)) ||
	    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (void*)&sbsz, sizeof(sbsz))) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: setsockopt: %s\n", fn, strerror(errno));
		goto out_err;
	}

#ifdef HAVE_SO_NOSIGPIPE
	if (setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nosigpipe, sizeof(int))) {
	  if (DEBUG_NI(d,PTL_DBG_NI_00)) {
	    p3_print("%s: setsockopt: %s\n", fn, strerror(errno));
	    goto out_err;
	  }
	}
#endif

	flags = fcntl(s, F_GETFL, 0);
	if (fcntl(s, F_SETFL, flags | O_NONBLOCK)) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: fcntl: %s\n", fn, strerror(errno));
		goto out_err;
	}
	if (connect(s, (void *) &chan->raddr, sz) < 0)
		if (errno != EINPROGRESS) {
			if (DEBUG_NI(d,PTL_DBG_NI_00))
				p3_print("%s: connect: %s\n", fn,
					 strerror(errno));
			goto out_err;
		}
	p3_lock(&d->dlock);
	FD_SET(s, &d->inuse_rd_fds);
	FD_SET(s, &d->inuse_wr_fds);
	FD_SET(s, &d->inuse_co_fds);
	d->max_fd = MAX(s, d->max_fd);
	p3_unlock(&d->dlock);

	if (DEBUG_NI(d,PTL_DBG_NI_03)) {
		sockaddr_in_t laddr = {0,};
		socklen_t addr_len;
		char *ip = inet_ntoa(chan->raddr.sin_addr);
		int lp, p = ntohs(chan->raddr.sin_port);

		addr_len = sizeof(laddr);
		if (!getsockname(chan->fd, (void *)&laddr, &addr_len))
			lp = ntohs(laddr.sin_port);
		else
			lp = ntohs(d->laddr.sin_port);

		p3_print("%s: chan %p start connect to %s:%d pid "FMT_PID_T
			 " on fd %d (src %s:%d pid "FMT_PID_T")\n",
			 fn, chan, ip, p, chan->rpid, s, d->laddr_str,
			 lp, chan->lpid);
		p3_flush();
	}
	return 0;
out_err:
	if (s >= 0)
		close(s);
	return -1;
}

static
p3tcp_chan_t *alloc_init_p3tcp_chan(void)
{
	p3tcp_chan_t *chan;

	if (!(chan = p3_malloc(sizeof(p3tcp_chan_t))))
		return NULL;

	memset(chan, 0, sizeof(*chan));
	INIT_LIST_HEAD(&chan->fd_hash);
	INIT_LIST_HEAD(&chan->nid_hash);
	INIT_LIST_HEAD(&chan->sendq);
	p3lock_init(&chan->sendq_lock);
	chan->fd = -1;

	return chan;
}

/* Call with d->dlock held.
 */
static
p3tcp_chan_t *p3tcp_new_chan(p3tcp_data_t *d, ptl_pid_t lpid,
			     ptl_nid_t rnid, ptl_pid_t rpid)
{
	static const char *fn = "p3tcp_new_chan";

	p3tcp_chan_t *chan;
	int p;

	if (!(chan = alloc_init_p3tcp_chan()))
		goto out;

	if (DEBUG_NI(d,PTL_DBG_NI_03))
		p3_print("p3tcp_new_chan: alloc new chan %p\n", chan);

	p = p3tcp_findport(d, rpid);

	if (p < 0 && d->defport) {
		p = d->defport[0];
		chan->use_defports = 1;
	}
	else goto out_err;

	chan->conn_retry = 1;

	chan->lpid = CHAN_LOCAL_PID(lpid);
	chan->rnid = rnid;
	chan->rpid = rpid;
	chan->raddr.sin_family = AF_INET;
	chan->raddr.sin_addr.s_addr = htonl(rnid);
	chan->raddr.sin_port = htons((short)p);

	p3tcp_put_chan(d, chan);

	/* Since we don't have an fd yet, we'll use chan->fd_hash
	 * to keep track of the channel until we initiate a connection.
	 */
	list_add_tail(&chan->fd_hash, &d->conn_wait);
	if (DEBUG_NI(d,PTL_DBG_NI_03))
		p3_print("%s: added chan %p to conn open list\n", fn, chan);

	return chan;

out_err:
	if (chan)
		p3_free(chan);
out:
	return NULL;
}

/* Returns 0 if able to successfully complete the connection, non-zero
 * if we need to try again later.
 */
static
int p3tcp_connect_cmpl(p3tcp_data_t *d, int fd, int rd)
{
	static const char *fn = "p3tcp_connect_cmpl";

	int i, p;
	p3tcp_chan_t *chan;
	socklen_t saddr_len = sizeof(chan->raddr);

	p3_lock(&d->dlock);
	chan = p3tcp_get_chan_fd(d, fd);
	p3_unlock(&d->dlock);

	if (!chan) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: channel not found\n", fn);
		goto out_retry;
	}
	if (timeout_set(&chan->conn_timeout)) {
		if (!test_timeout(&chan->conn_timeout))
			goto out_retry;
		clear_timeout(&chan->conn_timeout);
	}
	if (!chan->init_sent) {

		if (getpeername(fd, (void *) &chan->raddr, &saddr_len) < 0)
			goto out_notconn;
		if (DEBUG_NI(d,PTL_DBG_NI_02))
			p3_print("%s: fd %d peer %s:%d\n",
				 fn, fd, inet_ntoa(chan->raddr.sin_addr),
				 (int) ntohs(chan->raddr.sin_port));
		i = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			       (void*)&i, sizeof(i)))
			if (DEBUG_NI(d,PTL_DBG_NI_00))
				p3_print("%s: setsockopt: %s\n",
					 fn, strerror(errno));

		if (p3tcp_chan_sndinit(d, chan, fn)) {
			if (DEBUG_NI(d,PTL_DBG_NI_02))
				p3_print("%s: fd %d: sndinit failed\n",
					 fn, fd);
			goto next_port;
		}
		chan->init_sent = 1;
		goto out_retry;
	}
	if (!chan->init_recv && rd) {

		if (p3tcp_chan_rcvinit(d, chan, 1, fn)) {
			if (DEBUG_NI(d,PTL_DBG_NI_02))
				p3_print("%s: fd %d: rcvinit failed\n",
					 fn, fd);
			goto next_port;
		}
		chan->init_recv = 1;
		goto out_retry;
	}
	if (!chan->conn_init && chan->init_recv) {

		if (p3tcp_chan_sndinit(d, chan, fn)) {
			if (DEBUG_NI(d,PTL_DBG_NI_02))
				p3_print("%s: fd %d: sndinit2 failed\n",
					 fn, fd);
			goto next_port;
		}
		p3_lock(&d->dlock);
		p3_lock(&chan->sendq_lock);

		if (list_empty(&chan->sendq)) {
			FD_CLR(fd, &d->inuse_wr_fds);
			if (chan->sendq_len)
				PTL_ROAD();
		}
		p3_unlock(&chan->sendq_lock);

		d->oconn--;
		FD_CLR(fd, &d->inuse_co_fds);

		p3_unlock(&d->dlock);

		chan->conn_init = 1;

		if (DEBUG_NI(d,PTL_DBG_NI_01)) {
			sockaddr_in_t laddr = {0,};
			socklen_t addr_len;
			char *ip = inet_ntoa(chan->raddr.sin_addr);
			int lp, p = ntohs(chan->raddr.sin_port);

			addr_len = sizeof(laddr);
			if (!getsockname(fd, (void *)&laddr, &addr_len))
				lp = ntohs(laddr.sin_port);
			else
				lp = ntohs(d->laddr.sin_port);

			p3_print("connect chan %p fd %d %s:%d pid "FMT_PID_T
				 " src %s:%d pid "FMT_PID_T" sendq len %d\n",
				 chan, fd, ip, p, chan->rpid,
				 d->laddr_str, lp, chan->lpid,
				 chan->sendq_len);
		}
		return 0;
	}
out_retry:
	return -1;

	/* Select said something happened on the fd on which we're waiting
	 * for a connection to complete, but we're not connected.
	 */
out_notconn:
	if (DEBUG_NI(d,PTL_DBG_NI_00)) {
		if (errno == ENOTCONN) {
			i = getsockerr(fd);
			if (i == ECONNREFUSED) goto next_port;
			p3_print("%s: %s (remote %s:%d pid "FMT_PID_T")\n",
				 fn, strerror(i),
				 inet_ntoa(chan->raddr.sin_addr),
				 (int)ntohs(chan->raddr.sin_port),
				 chan->rpid);
		}
		else
			p3_print("%s: getpeername: %s\n", fn, strerror(errno));
	}

next_port:
	if (DEBUG_NI(d,PTL_DBG_NI_03))
		p3_print("%s: chan %p attempting connect on next port.\n",
			 fn, chan);

	/* See if there are any more default ports to try.  We want to
	 * close the socket, but not delete the channel, so call the
	 * helper.
	 */
	__tcp_close_chan(d, chan);

	if (!chan->use_defports ||
	    chan->defport_idx == d->ndport - 1) {

		unsigned msec_next;

		if (chan->conn_fail) {
			p3tcp_close_chan(d, chan, PTL_DBG_NI_02);
			goto out_retry;
		}
		msec_next = TCP_RETRY_DELAY * chan->conn_retry;
		set_timeout(&chan->conn_timeout, msec_next);

		if (chan->conn_retry == TCP_CONNECT_RETRIES)
			chan->conn_fail = 1;
		else
			chan->conn_retry *= 2;

		chan->defport_idx = 0;
	}
	else ++chan->defport_idx;

	p = d->defport[chan->defport_idx];
	chan->raddr.sin_port = htons((short)p);

	if (__p3tcp_connect(d, chan, fn))
		goto next_port;
	goto out_retry;
}

/* helper function for p3tcp_accept_cmpl() to create an new channel object.
 */
static inline
p3tcp_chan_t *__tcp_accept_alloc_chan(p3tcp_data_t *d, int fd, const char *fn)
{
	socklen_t addr_len;
	p3tcp_chan_t *chan;

	if (!(chan = alloc_init_p3tcp_chan()))
		return NULL;

	if (DEBUG_NI(d,PTL_DBG_NI_03))
		p3_print("%s: alloc new chan %p\n", fn, chan);

	chan->fd = fd;
	addr_len = sizeof(chan->raddr);

	if (getpeername(fd, (void *) &chan->raddr, &addr_len) < 0) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
		    p3_print("%s: getpeername: %s\n", fn, strerror(errno));
		p3_lock(&d->dlock);
		FD_CLR(fd, &d->inuse_rd_fds);
		p3_unlock(&d->dlock);
		close(fd);
		p3_free(chan);
		return NULL;
	}
	p3_lock(&d->dlock);
	p3tcp_put_chan_fd(d, chan);
	p3_unlock(&d->dlock);
	return chan;
}

/* Helper function for p3tcp_accept_cmpl() to check for a duplicate
 * channel object.  Returns nchan if we should add it to the list of
 * open channels, or otherwise deletes nchan and returns NULL.
 */
static
p3tcp_chan_t *__tcp_accept_new_chan(p3tcp_chan_t *nchan,
				    p3tcp_data_t *d, const char *fn)
{
	ptl_nid_t dnid = d->nid;
	p3tcp_chan_t *echan = NULL;	/* existing channel */

	/* There are two reasons there might be a duplicate channel in the
	 * list of open channels.
	 *
	 * One is the case of a process sending to itself, where the easiest
	 * thing to do is to have two channels, one for each end of the tcp
	 * connection.  Each of those channels will have local and remote
	 * PIDs that are the same, and each will have our NID.
	 *
	 * If we're sending to self or there is no duplicate, the new
	 * channel survives.
	 */
	p3_lock(&d->dlock);
	echan = p3tcp_get_chan(d, nchan->rnid, nchan->rpid);
	p3_unlock(&d->dlock);

	if (!echan ||
	    (nchan->rnid == dnid && nchan->lpid == nchan->rpid ))
		return nchan;

	if (DEBUG_NI(d,PTL_DBG_NI_02)) {
		p3_print("%s: new chan %p fd %d %s:%d pid "FMT_PID_T
			 " sendq len %d"
			 " dups chan %p fd %d %s:%d pid "FMT_PID_T
			 " sendq len %d\n", fn,
			 nchan, nchan->fd, inet_ntoa(nchan->raddr.sin_addr),
			 (int)ntohs(nchan->raddr.sin_port), nchan->rpid,
			 nchan->sendq_len,
			 echan, echan->fd, inet_ntoa(echan->raddr.sin_addr),
			 (int)ntohs(echan->raddr.sin_port), echan->rpid,
			 echan->sendq_len);
		p3_flush();
	}
	/* If there is a duplicate, existing channel, _we_ had to be the
	 * initiator on the duplicate, and we are racing with the initiator
	 * of this new connection to send to each other.  In this case, the
	 * initiator of the connection we just accepted must have a nid/pid
	 * different from ours, and the connection that survives is the one
	 * whose initiator has the highest nid, or highest pid if nids are
	 * the same.
	 */
	if (nchan->rnid < dnid ||
	    (nchan->rnid == dnid && nchan->rpid < echan->lpid)) {

		if (DEBUG_NI(d,PTL_DBG_NI_02)) {
			p3_print("%s: closing new duplicate chan fd %d"
				 " sendq len %d\n",
				 fn, nchan->fd, nchan->sendq_len);
		}
		p3_flush();
		p3_lock(&d->dlock);
		FD_CLR(nchan->fd, &d->inuse_rd_fds);
		if (p3tcp_get_chan_fd(d, nchan->fd))
			list_del(&nchan->fd_hash);
		p3_unlock(&d->dlock);
		close(nchan->fd);
		p3_free(nchan);

		return NULL;	/* The existing channel survives */
	}
	/* The existing connection (i.e., the one we initiated) is the
	 * duplicate, so we need to kill it.  The duplicate channel can
	 * be in the middle of the initial handshake, or it can be newly
	 * initialized but not yet started sending, but it cannot be
	 * receiving anything.  Otherwise, we wouldn't be in the middle
	 * of accepting a new connection.
	 */
	if (DEBUG_NI(d,PTL_DBG_NI_02)) {
		p3_print("%s: closing dup chan %p fd %d %s:%d pid "FMT_PID_T
			 " sendq len %d\n", fn, echan,
			 echan->fd, inet_ntoa(echan->raddr.sin_addr),
			 (int)ntohs(echan->raddr.sin_port), echan->rpid,
			 echan->sendq_len);
		p3_flush();
	}
	/* Now that we're committed to deleting this channel, grab the
	 * p3tcp_data lock to prevent a send from locating it and adding to
	 * the send queue while we're in the process of deleting it.
	 */
	p3_lock(&d->dlock);

	if (echan->recv) {
		p3_print("%s: trying to kill duplicate channel with active"
			 " receive from rnid "FMT_NID_T" rpid "FMT_PID_T"\n",
			 fn, echan->rnid, echan->rpid);
		p3_flush();
		PTL_ROAD();
	}
	if (!echan->conn_init) {
		d->oconn--;
		if (echan->fd >= 0)
			FD_CLR(echan->fd, &d->inuse_co_fds);

		if (DEBUG_NI(d,PTL_DBG_NI_03)) {
			p3_print("%s: dec oconn count, now %d\n",
				 fn, d->oconn);
			p3_flush();
		}
	}
	/* Move the duplicate channel's send queue to the tail of the
	 * surviving (new) channel.  This preserves ordering guarantees,
	 * since the new channel was created in response to a new incoming
	 * message (i.e., receive).
	 *
	 * We don't need a channel lock here because the p3tcp_data lock
	 * protects us against API access of the channel via p3tcp_send,
	 * and we're single threaded.
	 */
	list_splice_init(&echan->sendq, nchan->sendq.prev);
	nchan->sendq_len += echan->sendq_len;
	echan->sendq_len = 0;
	if (echan->fd >= 0 && FD_ISSET(echan->fd, &d->inuse_wr_fds))
		FD_SET(nchan->fd, &d->inuse_wr_fds);

	/* We've got to remove it from the hash lists ourself while we
	 * hold the lock - we can't risk dropping the lock to delete
	 * otherwise.
	 */
	list_del_init(&echan->fd_hash);
	list_del_init(&echan->nid_hash);

	p3_unlock(&d->dlock);

	p3tcp_close_chan(d, echan, PTL_DBG_NI_02);

	return nchan;	/* The new channel survives. */
}

/* call this after a successful accept() to receive some startup data
 * used for consistency checking.
 */
static
p3tcp_chan_t *p3tcp_accept_cmpl(p3tcp_data_t *d, int fd)
{
	static const char *fn = "p3tcp_accept_cmpl";
	p3tcp_chan_t *chan;

	p3_lock(&d->dlock);
	chan = p3tcp_get_chan_fd(d, fd);
	p3_unlock(&d->dlock);

	if (!chan && DEBUG_NI(d,PTL_DBG_NI_03))
		p3_print("%s: alloc new chan for fd %d\n", fn, fd);

	if (!(chan || (chan = __tcp_accept_alloc_chan(d, fd, fn))))
		return NULL;

	/* Receive the channel init handshake sent by initiator.
	 */
	if (!chan->init_recv) {
		if (p3tcp_chan_rcvinit(d, chan, 0, fn))
			goto out_bad;
		else
			chan->init_recv = 1;

		/* Make sure this channel is not a duplicate of
		   an existing channel.
		*/
		if (!__tcp_accept_new_chan(chan, d, fn))
			return NULL;
	}
	if (!chan->init_sent) {

		if (p3tcp_chan_sndinit(d, chan, fn))
			goto out_bad;

		chan->init_sent = 1;
		return NULL;
	}
	if (p3tcp_chan_rcvinit(d, chan, 0, fn))
		goto out_bad;

	/* Make sure (again) this channel is not a duplicate of
	   an existing channel.
	 */
	if (!__tcp_accept_new_chan(chan, d, fn))
		return NULL;

	if (DEBUG_NI(d,PTL_DBG_NI_01)) {
		p3_print("accept chan %p fd %d from %s:%d pid "FMT_PID_T
			 " sendq len %d\n", chan,
			 chan->fd, inet_ntoa(chan->raddr.sin_addr),
			 (int)ntohs(chan->raddr.sin_port), chan->rpid,
			 chan->sendq_len);
		p3_flush();
	}
	chan->conn_init = 1;
	p3tcp_put_chan(d, chan);
	return chan;

out_bad:
	/* This connection is no good. Clean up.
	 */
	if (DEBUG_NI(d,PTL_DBG_NI_02)) {
		p3_print("%s: closing bad connection fd %d\n", fn, fd);
	}
	p3_flush();
	p3_lock(&d->dlock);
	FD_CLR(fd, &d->inuse_rd_fds);
	if (p3tcp_get_chan_fd(d, fd))
		list_del(&chan->fd_hash);
	p3_unlock(&d->dlock);
	close(fd);
	p3_free(chan);

return NULL;
}

static int p3tcp_my_nid(const char *if_str, unsigned int *nid)
{
	/* If there are more than 100 interfaces/aliases there is bound to be
	 * serious trouble elsewhere.
	 */
	const int ifr_max = 100;
	struct ifreq *ifr;
	struct ifconf ifc;
	int rem, s, err = 0, length, found = 0;
	char *errstr = NULL, *ptr;

        ifc.ifc_len = ifr_max * sizeof(struct ifreq);
        ifc.ifc_req = p3_malloc(ifc.ifc_len);
	if (!ifc.ifc_req) {
		err = ENOMEM;
		errstr = "p3tcp_my_nid: malloc";
		goto abort;
	}
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		err = errno;
		errstr = "p3tcp_my_nid: socket";
		goto abort;
	}

	if (ioctl(s, SIOCGIFCONF, (unsigned long)&ifc) < 0) {
		err = errno;
		errstr = "p3tcp_my_nid: ioctl";
		goto abort;
	}
	if (close(s)) {
		err = errno;
		errstr = "p3tcp_my_nid: close";
		goto abort;
	}

        rem = ifc.ifc_len;
        ptr = (char*) ifc.ifc_req;
        while (rem > 0) {
            ifr = (struct ifreq*) ptr;

            if (strcmp(ifr->ifr_name, if_str) == 0) {
                found = 1;
                if (ifr->ifr_addr.sa_family == AF_INET) {
                    struct sockaddr_in *saddr;
                    saddr = (struct sockaddr_in *) &(ifr->ifr_addr);
                    *nid = ntohl(saddr->sin_addr.s_addr);
                    p3_free(ifc.ifc_req);
                    return 0;
                }
            }

#if defined(__MACOSX__) || defined(__APPLE__)
            length = sizeof(struct sockaddr);

            if (ifr->ifr_addr.sa_len > length) {
                length = ifr->ifr_addr.sa_len;
            }

            length += sizeof(ifr->ifr_name);
#else
            length = sizeof(struct ifreq);
#endif
            rem -= length;
            ptr += length;
	}
        if (found) {
            errstr = "device found, without IPv4 support";
            err = ENODEV;
        } else {
            errstr = "No such interface";
            err = ENODEV;
        }
abort:
	if (ifc.ifc_req)
		p3_free(ifc.ifc_req);
	p3_print("p3tcp_my_nid: trying %s: %s: %d\n", if_str, errstr, err);
	return -1;
}

/* Helper function, returns:
 *	 0	success,
 *	<0	failure due to address in use
 *	>0	failure for any other reason, returns errno.
 */
static inline
int __tcp_listen(p3tcp_data_t *d, int s, int reuse_addr)
{
	static const char *fn = "__tcp_listen";
	int rtn = -1;

	/* If reuse_addr is set, we know exactly which port we want to
	 * bind to, so we might as well use SO_REUSEADDR so we don't have
	 * to 1) wait for a socket from a previous job to leave the TCP
	 * TIME_WAIT state, or 2) use a different port from a previous job.
	 */
	if (reuse_addr) {
		int on = 1;
		if (DEBUG_NI(d,PTL_DBG_NI_02))
			p3_print("%s: reusing address\n", fn);
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
			       (void*)&on, sizeof(on))) {
			rtn = errno;
			if (DEBUG_NI(d,PTL_DBG_NI_00))
				p3_print("%s: setsockopt: %s\n",
					 fn, strerror(errno));
			goto out;
		}
	}
	if (DEBUG_NI(d,PTL_DBG_NI_03))
		p3_print("%s: attempt bind to %s:%d on fd %d\n",
			 fn, inet_ntoa(d->laddr.sin_addr),
			 (int) ntohs(d->laddr.sin_port), s);
	if (bind(s, (void *) &d->laddr, sizeof(d->laddr))) {
		if (errno == EADDRINUSE) goto out;
		rtn = errno;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: bind: %s\n", fn, strerror(errno));
		goto out;
	}
	if (listen(s, TCP_LISTEN_BACKLOG)) {
		if (errno == EADDRINUSE) goto out;
		rtn = errno;
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: listen: %s\n", fn, strerror(errno));
		goto out;
	}
	if (DEBUG_NI(d,PTL_DBG_NI_01))
		p3_print("%s: bound to %s:%d on fd %d\n",
			 fn, inet_ntoa(d->laddr.sin_addr),
			 (int) ntohs(d->laddr.sin_port), s);
	rtn = 0;
out:
	return rtn;
}

static
int p3tcp_listen(p3tcp_data_t *d)
{
	static const char *fn = "p3tcp_listen";

	int err = -1, s = -1;
	int flags, sbsz = TCP_SOCKBUF_SZ;
	int p = -1, scan = -1, reuse_addr = 1;
#ifdef HAVE_SO_NOSIGPIPE
	int nosigpipe = 1;
#endif

	d->laddr.sin_family = AF_INET;
	d->laddr.sin_addr.s_addr = htonl(d->nid);

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: socket: %s\n", fn, strerror(errno));
		goto out;
	}
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (void*)&sbsz, sizeof(sbsz)) ||
	    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (void*)&sbsz, sizeof(sbsz))) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: setsockopt: %s\n", fn, strerror(errno));
		goto out;
	}

#ifdef HAVE_SO_NOSIGPIPE
	if (setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nosigpipe, sizeof(int))) {
	  if (DEBUG_NI(d,PTL_DBG_NI_00)) {
	    p3_print("%s: setsockopt: %s\n", fn, strerror(errno));
	  }
	}
#endif

#ifndef PTL_KERNEL_BLD
	/* When we're a kernel-space NAL, we handle all PIDs, so we always
	 * listen on the default port.  For user-space, well-known PIDs have
	 * assigned ports, so see if we have a port other than the default.
	 */
	p = p3tcp_findport(d, d->pid);
	/* The PID stored in p3tcp_data_t is for startup purposes only.
	 * Reset it here so we find any buggy uses of it.
	 */
	d->pid = PTL_PID_ANY;
#endif
	if (p < 0 && d->defport) {
		p = d->defport[++scan];
		if (d->ndport > 1)
			reuse_addr = 0;
	}
	else
		goto out;

next_port:
	d->laddr.sin_port = htons((short)p);

	err = __tcp_listen(d, s, reuse_addr);
	if (err < 0) {
		if (++scan < (int)d->ndport) {
			p = d->defport[scan];
			goto next_port;
		}
		p3_print("%s: port not available\n", fn);
		goto out;
	}
	else if (err > 0) goto out;

	d->listen_fd = s;

	flags = fcntl(s, F_GETFL, 0);
	if (fcntl(s, F_SETFL, flags | O_NONBLOCK)) {
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("%s: fcntl: %s\n", fn, strerror(errno));
		goto out;
	}
	d->laddr_str = p3_strdup(inet_ntoa(d->laddr.sin_addr));
out:
	if (err && s >= 0)
		close(s);
	return err;
}

static
void p3tcp_accept(p3tcp_data_t *d)
{
	int s, on;
	struct sockaddr_in saddr;
	socklen_t saddr_len = sizeof(saddr);

	p3_lock(&d->dlock);
	on = d->up;
	p3_unlock(&d->dlock);
	if (!on)
		return;
again:
	if ((s = accept(d->listen_fd, (void *) &saddr, &saddr_len)) < 0) {
		if (errno == EINTR)
			goto again;
		return;
	}
	if (DEBUG_NI(d,PTL_DBG_NI_02))
		p3_print("p3tcp_accept: %s:%d on fd %d (src %s:%d)\n",
			 d->laddr_str, (int)ntohs(d->laddr.sin_port), s,
			 inet_ntoa(saddr.sin_addr),
			 (int)ntohs(saddr.sin_port));

	/* Note that we don't save the peer address here, because we don't
	 * want to create the channel struct yet.  We'll do that in
	 * p3tcp_accept_cmpl(), the first time the socket becomes readable.
	 */
	if (saddr.sin_family != AF_INET) close(s);
	else {
		p3_lock(&d->dlock);
		FD_SET(s, &d->inuse_rd_fds);
		d->max_fd = MAX(s, d->max_fd);
		p3_unlock(&d->dlock);
	}
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on)))
		if (DEBUG_NI(d,PTL_DBG_NI_00))
			p3_print("p3tcp_accept: setsockopt: %s\n",
				 strerror(errno));
	return;
}

/* Call only from process context on NI shutdown, as it may sleep.
 *
 * By this time the library has guaranteed that no new work will be
 * queued for this NI, and that any messages that arrive on channels
 * used by this NI will be dropped without association to this NI.
 *
 * ni == NULL means drain all channels.
 */
static void p3tcp_drain_chan(p3tcp_data_t *d, lib_ni_t *ni)
{
	unsigned i;
	p3tcp_msg_t *msg, *w_msg = NULL;
	p3tcp_chan_t *chan;
	struct list_head *fd_hash_head;
again:

#if !defined PTL_KERNEL_BLD && !defined USER_PROGRESS_THREAD
	p3tcp_chan_poll(d, 1);
#endif
	kdown(&d->poll_sem);
	p3_lock(&d->dlock);

	if (DEBUG_NI(d, PTL_DBG_NI_06) ||
	    DEBUG_P3(d->debug, PTL_DBG_SHUTDOWN))
		p3_print("p3tcp_drain_chan: try NI %p\n", ni);

	for (i=0; i<TCP_FD_HASH_BINS; i++) {
		fd_hash_head = d->chan_fd_hash + i;

		list_for_each_entry(chan, fd_hash_head, fd_hash) {

			msg = chan->recv;
			if (msg && (msg->ni == ni || !ni)) {
				p3_unlock(&d->dlock);
				if (msg != w_msg)
					w_msg = msg;

				kup(&d->poll_sem);
				goto again;
			}
			p3_lock(&chan->sendq_lock);

			list_for_each_entry(msg, &chan->sendq, queue) {
				if (msg->ni == ni || !ni) {
					p3_unlock(&chan->sendq_lock);
					p3_unlock(&d->dlock);
					kup(&d->poll_sem);
					p3tcp_dequeue_msg(msg->ni, msg);
					goto again;
				}
			}
			p3_unlock(&chan->sendq_lock);
		}
	}
	p3_unlock(&d->dlock);
	kup(&d->poll_sem);
	return;
}

void p3tcp_libnal_shutdown(p3tcp_data_t *d)
{
        unsigned i;
        p3tcp_chan_t *chan;

	if (DEBUG_NI(d,PTL_DBG_NI_03) ||
	    DEBUG_P3(d->debug, PTL_DBG_SHUTDOWN))
		p3_print("p3tcp_libnal_shutdown: waiting for outstanding "
			 "msgs\n");

	p3_lock(&d->dlock);
	d->up = 0;
	p3_unlock(&d->dlock);

#ifdef PTL_KERNEL_BLD
	p3tcp_drain_chan(d, NULL);
#endif
	if (DEBUG_NI(d,PTL_DBG_NI_03) ||
	    DEBUG_P3(d->debug, PTL_DBG_SHUTDOWN))
		p3_print("p3tcp_libnal_shutdown: "
			 "closing all open channels.\n");

        for (i=0; i<TCP_FD_HASH_BINS; i++) {
		struct list_head *head;
	again:
		p3_lock(&d->dlock);
		head = d->chan_fd_hash + i;
		if (!list_empty(head)) {
			chan = list_entry(head->next, p3tcp_chan_t, fd_hash);
			p3_unlock(&d->dlock);
			p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
			goto again;
		}
		p3_unlock(&d->dlock);
	}
	p3_lock(&d->dlock);
	if (d->listen_fd >= 0) {
		FD_CLR(d->listen_fd, &d->inuse_rd_fds);
		close(d->listen_fd);
		d->listen_fd = -1;
		p3_free(d->laddr_str);
	}
	p3_unlock(&d->dlock);
}

#ifdef PTL_KERNEL_BLD

typedef long (*move_func_t)(int, void*, size_t, unsigned int);

static
#if 0
inline
#endif
int __get_buf_page(lib_ni_t *ni,
		   void **addr,			/* where to receive */
		   ssize_t *nbytes,		/* how much to receive */
		   struct page **page,		/* page addr is on */
		   unsigned long len,		/* bytes left to receive */
		   unsigned long os,		/* offset in iov segment */
		   const ptl_md_iovec_t *iov_seg,/* iov segment to use */
		   struct p3_ubuf_map *seg)
{
	unsigned long dst_va = (unsigned long)iov_seg->iov_base;

	*nbytes = MIN((unsigned long)SSIZE_T_MAX,
		      MIN(len, (unsigned long)iov_seg->iov_len - os));

	/* seg will be non-NULL if iov_seg contains user addresses, and NULL
	 * if iov_seg contains kernel adresses.  However, even if seg is
	 * non-null, it will only be a valid address if NO_PINNED_PAGES
	 * is not defined.
	 */
	if (seg) {
		unsigned long pg_os = (dst_va + os) & (PAGE_SIZE-1);
		*nbytes = MIN((unsigned long)*nbytes, PAGE_SIZE - pg_os);

#ifdef NO_PINNED_PAGES
		{
			struct mm_struct *mm;
			struct task_struct *task = ni->owner->user_task;

			/* We'll hold the semaphore during the socket
			 * call, so the page can't move on us.
			 *
			 * Hopefully this is not a Real Bad Thing (tm).
			 *
			 * Also, unlike the normal termination process
			 * (i.e., process calls exit), if a process is
			 * aborted (e.g., ctrl-c), the task mm seems to
			 * get freed before lib_PtlNIFini gets called.
			 *
			 * If we happen to be receiving a message for that
			 * process at the time, we have to take special
			 * measures to handle it.
			 */
			mm = get_task_mm(task);

			if (!mm)
				return -EFAULT;

			down_read(&mm->mmap_sem);
			if (1 != get_user_pages(task, mm, dst_va + os,
						1, 1, 1, page, NULL)) {
				*page = NULL;
				up_read(&mm->mmap_sem);
				mmput(mm);
				return -EFAULT;
			}
		}
#else
		{
			unsigned long pg_no =
				((dst_va + os) >> PAGE_SHIFT) -
				(dst_va >> PAGE_SHIFT);

			if (seg->uaddr != dst_va ||
			    seg->length != iov_seg->iov_len)
				PTL_ROAD();

			*page = seg->sglist[pg_no]->page;
		}
#endif
		*addr = kmap_atomic(*page, KM_USER1) + pg_os;
	}
	else {
		*page = NULL;
		*addr = (void *)(dst_va + os);
	}
	return 0;
}

static inline
void __put_buf_page(lib_ni_t *ni, struct page *page, struct p3_ubuf_map *seg)
{
	if (seg) {
#ifdef NO_PINNED_PAGES
		struct task_struct *task = ni->owner->user_task;

		if (!task->mm)
			PTL_ROAD();
		up_read(&task->mm->mmap_sem);
		mmput(task->mm);
#endif
		kunmap_atomic(page, KM_USER1);
		flush_dcache_page(page);
#ifdef NO_PINNED_PAGES
		set_page_dirty_lock(page);
		put_page(page);
#endif
	}
}

static inline
struct p3_ubuf_map *__addrmap_seg(struct p3_addrmap *am, int i)
{
	struct p3_ubuf_map *map;
	if (am) {
#ifdef NO_PINNED_PAGES
		map = P3_MSG_ADDRMAP_NO_PIN;
#else
		if ((size_t)i >= am->maplen)
			PTL_ROAD();
		map = am->map[i];
#endif
	}
	else
		map = NULL;

	return map;
}

#else /* !PTL_KERNEL_BLD */

struct page {
	int pad;
};

typedef ssize_t (*move_func_t)(int, void*, size_t, int);

static inline
int __get_buf_page(lib_ni_t *ni,
		   void **addr,			/* where to receive */
		   ssize_t *nbytes,		/* how much to receive */
		   struct page **page,		/* page addr is on */
		   unsigned long len,		/* bytes left to receive */
		   unsigned long os,		/* offset in iov segment */
		   ptl_md_iovec_t *iov_seg,	/* iov segment to use */
		   struct p3_ubuf_map *seg)
{
	unsigned long dst_va = (unsigned long)iov_seg->iov_base;

	*nbytes = MIN((unsigned long)SSIZE_T_MAX,
		      MIN(len, (unsigned long)iov_seg->iov_len - os));

	*addr = (void *)(dst_va + os);
	return 0;
}

#define __put_buf_page(ni,page,seg) do {} while (0)

static inline
void *__addrmap_seg(void *am, int i) { return NULL; }

#endif /* !PTL_KERNEL_BLD */


static
ssize_t __sock_move(const char *mv_fn,
		    move_func_t move,
		    p3tcp_data_t *d,
		    lib_ni_t *ni,
		    int fd,
		    ptl_size_t len,		/* how many bytes to send */
		    ptl_size_t os,		/* where in iov to start */
		    ptl_md_iovec_t *iov,
		    ptl_size_t iovlen,
		    struct p3_addrmap *am)
{
	static const char *fn = "__sock_move";
	ssize_t r = 0;
	ptl_size_t seg_start, seg_next = 0;
	int i = -1, rc;

	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("%s: fd %d %s "FMT_PSZ_T" bytes, iovlen "
			 FMT_PSZ_T"\n", fn, fd, mv_fn, len, iovlen);

	if (len <= 0)
		return 0;

	/* What we want to do is move whatever will fit into the socket
	 * buffer, without trying to wait for more space.  But, we don't want
	 * to return early because we crossed from one iov segment to the next.
	 */
	while (++i < (int)iovlen) {

		ptl_size_t seg_os;
		void *addr;
		ssize_t nbytes;
		struct page *page;

		seg_start = seg_next;
		seg_next += iov[i].iov_len;
	again:
		if (os >= seg_next)
			continue;

		seg_os = os - seg_start;

		rc = __get_buf_page(ni, &addr, &nbytes, &page, len, seg_os,
				    &iov[i], __addrmap_seg(am, i));
		if (rc)
			return rc;

		nbytes = move(fd, addr, nbytes, 0);
		__put_buf_page(ni, page, __addrmap_seg(am, i));

		if (DEBUG_NI(d,PTL_DBG_NI_07))
			p3_print("%s: fd %d %s "FMT_SSZ_T" bytes"
				 " %p(%p) iov seg %d os "FMT_PSZ_T
				 " start %p os "FMT_PSZ_T
				 " next %p os "FMT_PSZ_T"\n",
				 fn, fd, mv_fn, nbytes,
				 iov[i].iov_base + seg_os, addr, i, os,
				 iov[i].iov_base, seg_start,
				 iov[i].iov_base + iov[i].iov_len, seg_next);

		if (nbytes > 0) {
			r += nbytes;
			os += nbytes;
			if (len -= nbytes)
				goto again;
			break;
		}
		if (nbytes < 0) {
			if (errno == EINTR)
				goto again;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;

			if (DEBUG_NI(d,PTL_DBG_NI_00))
				p3_print("%s: %s: %s\n",
					 fn, mv_fn, strerror(errno));
		}
		/* Select said socket was ready. Check for a pending error and
		 * signal to caller that our socket should be closed.
		 */
		if (DEBUG_NI(d,PTL_DBG_NI_00)) {
			i = getsockerr(fd);
			if (i)
				p3_print("%s: %s: %s\n",
					 fn, mv_fn, strerror(i));
		}
		r = -EPIPE;
		break;
	}
	return r;
}

/* This function should only be called if select reports the fd as ready
 * for reading.  If we return -1 the socket should be closed.
 */
static
ssize_t p3tcp_sock_rcv(p3tcp_data_t *d,
		       lib_ni_t *ni,
		       int fd,
		       ptl_size_t len,	/* how many bytes to receive */
		       ptl_size_t os,	/* where in iov to start */
		       ptl_md_iovec_t *iov,
		       ptl_size_t iovlen,
		       struct p3_addrmap *am)
{
	static const char *mv_fn = "recv";

	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("p3tcp_sock_rcv: fd %d "FMT_PSZ_T" bytes, iovlen "
			 FMT_PSZ_T"\n", fd, len, iovlen);

	return __sock_move(mv_fn, recv,
			   d, ni, fd, len, os, iov, iovlen, am);
}

/* This function should only be called if select reports the fd as ready
 * for writing.  If we return -1 the socket should be closed.
 */
static
ssize_t p3tcp_sock_snd(p3tcp_data_t *d,
		       lib_ni_t *ni,
		       int fd,
		       ptl_size_t len,	/* how many bytes to send */
		       ptl_size_t os,	/* where in iov to start */
		       ptl_md_iovec_t *iov,
		       ptl_size_t iovlen,
		       struct p3_addrmap *am)
{
	static const char *mv_fn = "send";

	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("p3tcp_sock_snd: fd %d "FMT_PSZ_T" bytes, iovlen "
			 FMT_PSZ_T"\n", fd, len, iovlen);

	/* This cast is a bug invitation, but I'd rather do this
	 * than have two copies of __sock_move, when the only difference
	 * is what function is called.
	 */
	return __sock_move(mv_fn, (move_func_t)send,
			   d, ni, fd, len, os, iov, iovlen, am);
}

static
void p3tcp_recvfrom(p3tcp_data_t *d, int fd)
{
	static const char *fn = "p3tcp_recvfrom";
	ptl_ni_fail_t ni_stat = PTL_NI_OK;

	p3tcp_chan_t *chan;
	p3tcp_msg_t *msg;

	p3_lock(&d->dlock);
	chan = p3tcp_get_chan_fd(d, fd);
	p3_unlock(&d->dlock);

	/* When a process is sending to another process on the same node,
	 * we can't start reading data immediately after accepting the
	 * connection.  We have to process the other endpoint first, so
	 * we can shove some data into it.
	 */
	if (!(chan && chan->conn_init)) {
		p3tcp_accept_cmpl(d, fd);
		return;
	}
	/* Select may mark the socket as readable because something
	 * happened (e.g. other end closed its socket).  Peek to see if
	 * there's any data; if not, close the channel.  This avoids
	 * erroneously creating a msg that will be destroyed when the
	 * channel is closed, causing us to signal an NI error.
	 */
	{
		char b;
		if (recv(fd, &b, 1, MSG_PEEK) == 0) {
			p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
			return;
		}
	}

	/* If we're not currently receiving anything, we need to pull a
	 * portals header off the socket and send it to the library. If
	 * so we also need to build the message struct that will keep
	 * track of the receive.
	 */
	if (!chan->recv) {
		size_t sz = sizeof(p3tcp_msg_t) + sizeof(ptl_hdr_t);

		if (!(msg = p3tcp_new_msg(d, sz)))
			return;
		if (DEBUG_NI(d,PTL_DBG_NI_05))
			p3_print("%s: fd %d malloc msg %p\n",
				 fn, chan->fd, msg);
		msg->hdr = (void *) msg + sizeof(p3tcp_msg_t);
		msg->hdrlen = sizeof(ptl_hdr_t);
		msg->chan = chan;
		chan->recv = msg;
	}
	else
		msg = chan->recv;

	/* We might be in the middle of receiving the header. Try to complete
	 * it.
	 */
	if (msg->hdros < msg->hdrlen) {
		ssize_t n;
		ptl_size_t m;
		ptl_md_iovec_t hdr_iov = {msg->hdr, msg->hdrlen};

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: start hdr recv fd %d msg %p\n",
				 fn, chan->fd, msg);

		n = p3tcp_sock_rcv(d, msg->ni, chan->fd,
				   msg->hdrlen - msg->hdros, msg->hdros,
				   &hdr_iov, 1, NULL);
		if (n < 0) {
			if (n == -EPIPE) {
				p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
				return;
			}
			else
				PTL_ROAD();
		}
		msg->hdros += n;
		if (msg->hdros != msg->hdrlen)
			return;

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: done hdr recv fd %d msg %p\n",
				 fn, chan->fd, msg);

		/* FIXME: consider the possibility of closing the channel
		 * if lib_parse fails, as we have no good way to resync
		 * our state with what's on the wire, assuming this really
		 * is a good channel.  In the meantime, we'll drop bytes
		 * as told by lib_parse().
		 */
		if (lib_parse(msg->hdr,
			      p3tcp_msg_id(msg), d->type, &m) == PTL_OK)
			return;
		else
			msg->droplen = m;
	}
	/* Otherwise, we might be in the middle of receiving message bytes.
	 * Try to complete that.
	 */
	if (msg->dataos < msg->datalen) {
		ssize_t n;

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: start data recv fd %d msg %p\n",
				 fn, chan->fd, msg);

		/* If we have a kernel-space API, msg_addrmap will be NULL.
		 */
		n = p3tcp_sock_rcv(d, msg->ni, chan->fd,
				   msg->datalen - msg->dataos,
				   msg->iovos + msg->dataos,
				   msg->iov, msg->iovlen,
				   P3_ADDRMAP_ADDRKEY(msg));
		if (n < 0) {
			if (n == -EPIPE) {
				p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
				return;
			}
			else if (n == -EFAULT) {
				/* The library is expecting us to finalize
				 * this message.
				 */
				msg->droplen += msg->datalen - msg->dataos;
				msg->datalen = msg->dataos;
				n = 0;
				ni_stat = PTL_NI_FAIL;
			}
		}
		msg->dataos += n;
		if (msg->dataos < msg->datalen)
			return;

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: done data recv fd %d msg %p\n",
				 fn, chan->fd, msg);
	}
	/* Otherwise, we might be in the middle of receiving junk bytes.
	 * Try to complete that.
	 */
	while (msg->dropos < msg->droplen) {
		ssize_t n;
		static char b[2048];
		ptl_md_iovec_t iov = {b, 2048};

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: start drop recv fd %d msg %p\n",
				 fn, chan->fd, msg);


		n = p3tcp_sock_rcv(d, msg->ni, chan->fd,
				   msg->droplen - msg->dropos,
				   0, &iov, 1, NULL);
		if (n < 0) {
			if (n == -EPIPE) {
				p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
				return;
			}
			else
				PTL_ROAD();
		}
		msg->dropos += n;

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: done drop recv fd %d msg %p\n",
				 fn, chan->fd, msg);
	}
	/* Otherwise, we must be done.
	 */
	p3_addrmap_hook_release(msg);

	if (DEBUG_NI(d,PTL_DBG_NI_05))
		p3_print("%s: fd %d "FMT_PSZ_T" hdr "FMT_PSZ_T" data "
			 FMT_PSZ_T" drop bytes from %s pid "FMT_PID_T"\n",
			 fn, chan->fd, msg->hdros, msg->dataos,
			 msg->dropos, inet_ntoa(chan->raddr.sin_addr),
			 chan->rpid);

	chan->recv = NULL;

	if (msg->libdat)
		lib_finalize(msg->ni, msg->libdat, ni_stat);

	if (DEBUG_NI(d,PTL_DBG_NI_05))
		p3_print("%s: fd %d recv idle\n", fn, chan->fd);

	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("%s: fd %d freeing msg %p\n", fn, chan->fd, msg);
	p3tcp_free_msg(d, msg);
}

static
void __sendjunk(p3tcp_data_t *d, lib_ni_t *ni,
		p3tcp_chan_t *chan, ptl_size_t nbytes)
{
	static const ssize_t bsz = 2048;
	static const char b[2048];

	ptl_md_iovec_t iov = {(void*)b, bsz};
	ssize_t n;

	while (nbytes) {
		n = p3tcp_sock_snd(d, ni, chan->fd, nbytes, 0, &iov, 1, NULL);
		if (n < 0) {
			if (n == -EPIPE) {
				p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
				return;
			}
			else
				PTL_ROAD();
		}
		nbytes -= n;
	}
}

static
void p3tcp_sendto(p3tcp_data_t *d, int fd)
{
	static const char *fn = "p3tcp_sendto";
printf("%s():%d\n",__func__,__LINE__);
	p3tcp_msg_t *msg, *next_msg;
	p3tcp_chan_t *chan;
	ptl_ni_fail_t ni_stat = PTL_NI_OK;

	p3_lock(&d->dlock);
	chan = p3tcp_get_chan_fd(d, fd);
	p3_unlock(&d->dlock);

	if (!(chan && chan->conn_init)) {
		if (DEBUG_NI(d,PTL_DBG_NI_08))
			p3_print("%s: cannot send: chan %p conn_init %d\n",
				 fn, chan, (chan ? chan->conn_init : 0));
		return;
	}
	/* We need to be holding d->dlock to clear the wr_inuse bit, but our
	 * locking rule says we have to acquire dlock before the sendq lock.
	 * So, we'll clear the wr_inuse bit here rather than after we drain
	 * the sendq, on the theory that we'll need to acquire dlock slightly
	 * less often (once per p3tcp_sendto call rather than once per message
	 * per call).
	 */
	p3_lock(&d->dlock);
	p3_lock(&chan->sendq_lock);
	if (list_empty(&chan->sendq)) {
		FD_CLR(fd, &d->inuse_wr_fds);
		p3_unlock(&chan->sendq_lock);
		p3_unlock(&d->dlock);
		return;
	}
	next_msg = list_entry(chan->sendq.next, p3tcp_msg_t, queue);
	p3_unlock(&chan->sendq_lock);
	p3_unlock(&d->dlock);

again:
	msg = next_msg;

	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("%s: chan %p fd %d sendq len %d\n",
			 fn, chan, chan->fd, chan->sendq_len);
	if (DEBUG_NI(d,PTL_DBG_NI_05))
		p3_print("%s: chan %p fd %d sending msg %p hdrlen "FMT_PSZ_T
			 " hdros "FMT_PSZ_T" len "FMT_PSZ_T" dataos "
			 FMT_PSZ_T"\n", fn, chan, chan->fd, msg, msg->hdrlen,
			 msg->hdros, msg->datalen, msg->dataos);

	/* We might be in the middle of sending the header. Try to complete
	 * it.
	 */
	if (msg->hdros < msg->hdrlen) {
		ssize_t n;
		ptl_md_iovec_t hdr_iov = {msg->hdr, msg->hdrlen};

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: fd %d sending hdr msg %p\n",
				 fn, chan->fd, msg);

		n = p3tcp_sock_snd(d, msg->ni, chan->fd,
				   msg->hdrlen - msg->hdros, msg->hdros,
				   &hdr_iov, 1, NULL);

		if (n < 0) {
			if (n == -EPIPE) {
				p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
				return;
			}
			else
				PTL_ROAD();
		}
		msg->hdros += n;
		if (msg->hdros < msg->hdrlen)
			return;

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: fd %d hdr done msg %p\n",
				 fn, chan->fd, msg);
	}
	/* Otherwise, we might be in the middle of sending message bytes.
	 * Try to complete that.
	 */
	if (msg->dataos < msg->datalen) {
		ssize_t n;
		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: fd %d sending data msg %p\n",
				 fn, chan->fd, msg);

		/* If we have a kernel-space API, msg_addrmap will be NULL
		 */
		n = p3tcp_sock_snd(d, msg->ni, chan->fd,
				   msg->datalen - msg->dataos,
				   msg->iovos + msg->dataos,
				   msg->iov, msg->iovlen,
				   P3_ADDRMAP_ADDRKEY(msg));

		if (n < 0) {
			if (n == -EPIPE) {
				p3tcp_close_chan(d, chan, PTL_DBG_NI_01);
				return;
			}
			else if (n == -EFAULT) {
				/* Whoooboy.  We still need to send
				 * msg->datalen - msg->dataos bytes, or our
				 * partner will get very confused. But, our
				 * source disappeared out from under us
				 * while we were sending.  Take steps.
				 */
				__sendjunk(d, msg->ni, chan,
					   msg->datalen - msg->dataos);
				msg->dataos = msg->datalen;
				ni_stat = PTL_NI_FAIL;
				n = 0;
			}
		}
		msg->dataos += n;
		if (msg->dataos < msg->datalen)
			return;

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: fd %d data done msg %p\n",
				 fn, chan->fd, msg);
	}
	/* Otherwise, we must be done.
	 */
	p3_addrmap_hook_release(msg);

	if (DEBUG_NI(d,PTL_DBG_NI_05))
		p3_print("%s: fd %d "FMT_PSZ_T" hdr "FMT_PSZ_T
			 " data bytes to %s pid "FMT_PID_T"\n",
			 fn, chan->fd, msg->hdros, msg->dataos,
			 inet_ntoa(chan->raddr.sin_addr), chan->rpid);

	p3_lock(&chan->sendq_lock);

	list_del(&msg->queue);
	chan->sendq_len--;

	if (list_empty(&chan->sendq))
		next_msg = NULL;
	else
		next_msg = list_entry(chan->sendq.next, p3tcp_msg_t, queue);

	p3_unlock(&chan->sendq_lock);

	lib_finalize(msg->ni, msg->libdat, ni_stat);

	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("%s: fd %d freeing msg %p\n", fn, chan->fd, msg);
	p3tcp_free_msg(d, msg);

	if (next_msg)
		goto again;

	if (DEBUG_NI(d,PTL_DBG_NI_05))
		p3_print("%s: fd %d snd idle\n", fn, chan->fd);
}

/* This function (thread) wakes up when there is data to move, or a
 * new request to dispatch.  It moves data in/out of sockets until it
 * can't anymore. If using a progress thread it then goes to sleep until
 * there is more work to do.
 */
pthread_mutex_t eq_poll_cond_lock;
pthread_cond_t eq_poll_cond;
volatile int eq_poll_cond_init=0;
volatile int eq_poll_event_pending = 0;

void p3tcp_chan_poll(p3tcp_data_t *d, int poll_cnt)
{
	static const char *fn = "tcpnal_chan_poll";
	static fd_set rfd_set, wfd_set, cfd_set;

	int fd, nr, nfds, k = poll_cnt;
	struct timeval *to = NULL;

	if(!eq_poll_cond_init) {
		eq_poll_cond_init = 1;
#ifdef USER_PROGRESS_THREAD
		pthread_mutex_init(&eq_poll_cond_lock, NULL);
		pthread_cond_init(&eq_poll_cond, NULL);
#endif

	}



#if defined USER_PROGRESS_THREAD
	int stop = 0;
#elif !defined PTL_KERNEL_BLD
	struct timeval _to = { 0, };
	to = &_to;
#endif

again:
	/* check for channels waiting to be connected.
	 */
	p3_lock(&d->dlock);

	while (!list_empty(&d->conn_wait)) {
		p3tcp_chan_t *chan = list_entry(d->conn_wait.next,
						p3tcp_chan_t, fd_hash);
		p3_unlock(&d->dlock);

		if (DEBUG_NI(d,PTL_DBG_NI_03))
			p3_print("%s: found chan %p on conn open list\n",
				 fn, chan);

		if (__p3tcp_connect(d, chan, fn)) {
			p3tcp_close_chan(d, chan, PTL_DBG_NI_02);
			p3_lock(&d->dlock);
			continue;
		}
		p3_lock(&d->dlock);
		d->oconn++;
		list_del(&chan->fd_hash);
		p3tcp_put_chan_fd(d, chan);
	}
	rfd_set = d->inuse_rd_fds;
	wfd_set = d->inuse_wr_fds;
	cfd_set = d->inuse_co_fds;
	nfds = 1 + d->max_fd;

	p3_unlock(&d->dlock);

	if (poll_cnt && k-- == 0)
		return;

	if (DEBUG_NI(d,PTL_DBG_NI_08))
		p3_print("%s: checking %d fds for activity\n", fn, nfds);

	/* FIXME: if we end up using this in a large cluster, we need
	 * something more efficient than select() if we have lots of sockets,
	 * but only a few go active at a time.
	 */
	if ((nr = select(nfds, &rfd_set, &wfd_set, NULL, to)) < 0) {
		if (errno == EINTR)
			goto again;
		err_abort(errno, "p3tcp_chan_poll: select");
	}
	else if (nr == 0)
		goto again;

	if (DEBUG_NI(d,PTL_DBG_NI_08))
		p3_print("%s: %d fds ready\n", fn, nr);

	if (d->listen_fd >= 0 && FD_ISSET(d->listen_fd, &rfd_set)) {
		FD_CLR(d->listen_fd, &rfd_set);
		p3tcp_accept(d);
	}
#ifdef USER_PROGRESS_THREAD
	if (FD_ISSET(d->rcv_wakeup, &rfd_set)) {
		char c;
		if (read(d->rcv_wakeup, &c, sizeof(c)) < 0 && errno != EAGAIN)
			err_abort(errno, "p3tcp_chan_poll: read rcv_wakeup");

		FD_CLR(d->rcv_wakeup, &rfd_set);
		if (!c)
			stop = 1;
	}
#endif
	kdown(&d->poll_sem);
	for (fd = 0; fd < nfds && nr; fd++) {
		int rd, wr, co;

		rd = FD_ISSET(fd, &rfd_set) ? 1 : 0;
		wr = FD_ISSET(fd, &wfd_set) ? 1 : 0;
		co = FD_ISSET(fd, &cfd_set) ? 1 : 0;

		if (DEBUG_NI(d,PTL_DBG_NI_08))
			p3_print("%s: nactive %d fd %d rd %d wr %d co %d.\n",
				 fn, nr, fd, rd, wr, co);
		nr -= rd;
		nr -= wr;

		/* Check for connect completion.  If a connect failed, select
		 * would show the socket as readable also, because of the
		 * error condition.  p3tcp_connect_cmpl() handles it all.
		 */
		if (d->oconn && co && (rd | wr)) {
			if (DEBUG_NI(d,PTL_DBG_NI_08))
				p3_print("%s: checking completion on fd %d\n",
					 fn, fd);
			if (p3tcp_connect_cmpl(d, fd, rd))
				rd = wr = 0;
			else {
				/* The connection is complete, but we need
				 * to see if there is any data waiting to be
				 * read, or we might create a msg that we
				 * shouldn't create yet.
				 */
				char b;
				if (recv(fd, &b, 1, MSG_PEEK) <= 0) rd = 0;
			}

		}
		if (rd)
			p3tcp_recvfrom(d, fd);
		if (wr)
			p3tcp_sendto(d, fd);

	}
	kup(&d->poll_sem);
#ifdef USER_PROGRESS_THREAD
	if (stop) {
		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: waking API thread.\n", fn);


		pthread_mutex_lock(&d->shutdown_sync.lock);
		d->shutdown_sync.flag = 1;
		pthread_mutex_unlock(&d->shutdown_sync.lock);
		pthread_cond_signal(&d->shutdown_sync.wait);

		return;
	}
#endif

#ifdef POLL_USE_COND_WAIT
	pthread_mutex_lock(&eq_poll_cond_lock);
	eq_poll_event_pending++;
	fprintf(stderr, "%lu: signaling eq_poll_cond (pending=%d)\n",
	        pthread_self(), eq_poll_event_pending);
	pthread_cond_signal(&eq_poll_cond);
	pthread_mutex_unlock(&eq_poll_cond_lock);
#endif

	goto again;
}

/* This function is called to acquire the listen socket.  If using a
 * progress thread, also runs the main event loop.
 */
static void *p3tcp_lib_start(void *arg)
{
	long rc;
	p3tcp_data_t *nal_data = arg;

	if ((rc = p3tcp_listen(nal_data)))
		return (void *)rc;

	p3_lock(&nal_data->dlock);
#ifdef USER_PROGRESS_THREAD
	FD_SET(nal_data->rcv_wakeup, &nal_data->inuse_rd_fds);
	nal_data->max_fd = MAX(nal_data->rcv_wakeup, nal_data->max_fd);
#endif
	FD_SET(nal_data->listen_fd, &nal_data->inuse_rd_fds);
	nal_data->max_fd = MAX(nal_data->listen_fd, nal_data->max_fd);
	p3_unlock(&nal_data->dlock);

#ifdef USER_PROGRESS_THREAD
	p3tcp_chan_poll(nal_data, 0);
#endif
	return NULL;
}

/* Depending on what type of Portals we're compiled for, we have to kick
 * a send in different ways to get it going with minimum latency.
 * Abstract that out here.
 */
static inline
void p3tcp_send_start(p3tcp_data_t *d, lib_ni_t *ni)
{
printf("%s():%d\n",__func__,__LINE__);
	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("p3tcp_send_start: Waking progress thread.\n");

#if defined PTL_KERNEL_BLD

	/* We have a kernel-space progress thread; wake it up.
	 */
	p3tcp_wakeup(d->progress_thread);

	if(DEBUG_NI(d, PTL_DBG_NI_06))
		p3_print("p3tcp_wakeup: process %p "
			 "signalled progress thread %p\n",
			 current, d->progress_thread->progress_task);

#elif defined USER_PROGRESS_THREAD

	/* We have a user-space progress thread; wake it up.
	 */
	{
		char c = 1;

		if (!write(d->snd_wakeup, &c, 1))
			err_abort(EIO,
				  "p3tcp_send_start: waking progress thread");
		p3_yield();
	}
#else
	/* We are single-threaded user-space; poll once to start send.
	 */
	p3tcp_chan_poll(ni->nal->private, 1);
#endif
	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("p3tcp_send_start: done.\n");
}

static
int p3tcp_send(lib_ni_t *ni, unsigned long *nal_msg_data, void *lib_data,
	       ptl_process_id_t dst, lib_mem_t *hdr, ptl_size_t hdrlen,
	       ptl_md_iovec_t *src_iov, ptl_size_t iovlen,
	       ptl_size_t offset, ptl_size_t len, void *addrkey)
{
	static const char *fn = "p3tcp_send";
printf("%s():%d\n",__func__,__LINE__);

	p3tcp_msg_t *msg;
	p3tcp_chan_t *chan;
	p3tcp_data_t *d = ni->nal->private;

	if (!(msg = p3tcp_new_msg(d, sizeof(p3tcp_msg_t))))
		err_abort(errno, "p3tcp_send: malloc");
	if (DEBUG_NI(d,PTL_DBG_NI_05))
		p3_print("%s: malloc msg %p\n", fn, msg);

	*nal_msg_data = p3tcp_msg_id(msg);

	msg->libdat = lib_data;
	msg->hdr = hdr;
	msg->hdros = 0;
	msg->hdrlen = hdrlen;

	msg->iov = src_iov;
	msg->iovlen = iovlen;
	msg->iovos = offset;

	msg->dataos = 0;
	msg->datalen = len;
	msg->dropos = 0;
	msg->droplen = 0;

	msg->ni = ni;

	p3_addrmap_hook_add_ref(msg, addrkey);

	/* Protect against __tcp_accept_new_chan() deleting a channel
	 * out from under us.
	 */
	p3_lock(&d->dlock);

	if (!((chan = p3tcp_get_chan(d, dst.nid, dst.pid)) ||
		    (chan = p3tcp_new_chan(d, ni->pid, dst.nid, dst.pid))))
		err_abort(EINVAL, "p3tcp_send: get channel");

	if (DEBUG_NI(d,PTL_DBG_NI_04))
		p3_print("%s: send "FMT_PSZ_T" bytes from" FMT_NIDPID
			 " to" FMT_NIDPID" on fd %d\n", fn, len,
			 ni->nid, ni->pid, dst.nid, dst.pid, chan->fd);
	msg->chan = chan;

	p3_lock(&chan->sendq_lock);
	chan->sendq_len++;
	list_add_tail(&msg->queue, &chan->sendq);

	if (chan->fd >= 0)
		FD_SET(chan->fd, &d->inuse_wr_fds);

	p3_unlock(&chan->sendq_lock);
	p3_unlock(&d->dlock);

	if (DEBUG_NI(d,PTL_DBG_NI_06))
		p3_print("%s: queued msg %p len "FMT_PSZ_T" dataos "FMT_PSZ_T
			 " sendq len %d\n",
			 fn, msg, msg->datalen, msg->dataos, chan->sendq_len);
	p3_flush();
	p3tcp_send_start(d, ni);

	return PTL_OK;
}

static
int p3tcp_recv(lib_ni_t *ni, unsigned long nal_msg_data, void *lib_data,
	       ptl_md_iovec_t *dst_iov, ptl_size_t iovlen, ptl_size_t offset,
	       ptl_size_t mlen, ptl_size_t rlen, void *addrkey)
{
	static const char *fn = "p3tcp_recv";
	p3tcp_data_t *d = ni->nal->private;
	p3tcp_msg_t *msg = p3tcp_get_msg(d, nal_msg_data);
	p3tcp_chan_t *chan;

	if (!msg)
		err_abort(EINVAL, "p3tcp_recv: get_message");

	p3_lock(&d->dlock);
	chan = p3tcp_get_chan_fd(d, msg->chan->fd);
	p3_unlock(&d->dlock);

	if (!chan)
		err_abort(EINVAL, "p3tcp_recv: get channel");

	if (DEBUG_NI(d,PTL_DBG_NI_04))
		p3_print("%s: recv "FMT_PSZ_T" bytes, "
			 "drop "FMT_PSZ_T " bytes to" FMT_NIDPID
			 " from" FMT_NIDPID" on fd %d\n",
			 fn, mlen, rlen - mlen, ni->nid, ni->pid,
			 chan->rnid, chan->rpid, chan->fd);
	if (DEBUG_NI(d,PTL_DBG_NI_05))
		p3_print("%s: recv "FMT_PSZ_T" bytes, drop "FMT_PSZ_T
			 " bytes msg_data %#lx msg %p\n",
			 fn, mlen, rlen - mlen, nal_msg_data, msg);
	if (chan->recv != msg) {
		p3_print("%s: chan->recv %p msg %p mlen "FMT_PSZ_T
			 " rlen "FMT_PSZ_T"\n",
			 fn, chan->recv, msg, mlen, rlen);
		err_abort(EINVAL, "p3tcp_recv: message");
	}
	if (rlen) {
		msg->libdat = lib_data;
		msg->iov = dst_iov;
		msg->iovlen = iovlen;
		msg->iovos = offset;

		msg->dataos = 0;
		msg->datalen = mlen;

		msg->dropos = 0;
		msg->droplen = rlen - mlen;

		msg->chan = chan;
		msg->ni = ni;

		p3_addrmap_hook_add_ref(msg, addrkey);
	}
	else {
		chan->recv = NULL;

		if (DEBUG_NI(d,PTL_DBG_NI_05))
			p3_print("%s: fd %d recv idle\n", fn, chan->fd);

		lib_finalize(ni, lib_data, PTL_NI_OK);

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("%s: freed msg %p\n", fn, msg);

		p3tcp_free_msg(d, msg);
	}
	return PTL_OK;
}

int p3tcp_dequeue_msg(lib_ni_t *ni, void *nal_internal_msg_struct)
{
	p3tcp_data_t *d = ni->nal->private;
	p3tcp_msg_t *msg = nal_internal_msg_struct;
	p3tcp_chan_t *chan = msg->chan;
	int rc = PTL_FAIL;

	if (ni != msg->ni)
		PTL_ROAD();

	p3_lock(&chan->sendq_lock);

	if (msg == chan->recv || &msg->queue == chan->sendq.next) {
		p3_unlock(&chan->sendq_lock);
		goto out;
	}
	__dequeue_free_msg("p3tcp_dequeue_msg", d, chan, msg);
	rc = PTL_OK;
out:
	return rc;
}

#ifdef PTL_KERNEL_BLD

int p3tcp_memcpy(lib_ni_t *ni,
		 ptl_size_t copy_len,
		 const ptl_md_iovec_t *src_iov_seg,
		 ptl_size_t src_seg_os,
		 ptl_size_t src_seg_idx,
		 void *src_addrkey,
		 ptl_md_iovec_t *dst_iov_seg,
		 ptl_size_t dst_seg_os,
		 ptl_size_t dst_seg_idx,
		 void *dst_addrkey)
{
	int rc = PTL_OK;
	void *src_addr, *dst_addr;
	ssize_t nbytes, src_nbytes, dst_nbytes;
	struct page *src_page, *dst_page;

	while (copy_len) {

		/* FIXME: if we cannot get an address something is very
		 * wrong, and we should do something besides return here.
		 * But what to do?
		 */
		if (__get_buf_page(ni, &src_addr, &src_nbytes, &src_page,
				   copy_len, src_seg_os, src_iov_seg,
				   __addrmap_seg(src_addrkey, src_seg_idx))) {
			rc = PTL_SEGV;
			break;
		}
		if (__get_buf_page(ni, &dst_addr, &dst_nbytes, &dst_page,
				   copy_len, dst_seg_os, dst_iov_seg,
				   __addrmap_seg(dst_addrkey, dst_seg_idx))) {
			__put_buf_page(ni, src_page,
				       __addrmap_seg(src_addrkey,
						     src_seg_idx));
			rc = PTL_SEGV;
			break;
		}
		nbytes = MIN(src_nbytes, dst_nbytes);
		memcpy(dst_addr, src_addr, nbytes);
		__put_buf_page(ni, src_page,
			       __addrmap_seg(src_addrkey, src_seg_idx));
		__put_buf_page(ni, dst_page,
			       __addrmap_seg(dst_addrkey, dst_seg_idx));

		copy_len -= nbytes;
	}
	return rc;
}

void *p3tcp_msg_container_of(lib_ni_t *ni, struct p3_addrmap_hook *hook)
{
	return msg_containing_hook(p3tcp_msg_t, hook);
}

#else

static int p3tcp_memcpy(lib_ni_t *ni,
		 ptl_size_t copy_len,
		 const ptl_md_iovec_t *src_iov_seg,
		 ptl_size_t src_seg_os,
		 ptl_size_t src_seg_idx,
		 void *src_addrkey,
		 ptl_md_iovec_t *dst_iov_seg,
		 ptl_size_t dst_seg_os,
		 ptl_size_t dst_seg_idx,
		 void *dst_addrkey)
{
	void *src_addr;
	api_mem_t *dst_addr;

	src_addr = src_iov_seg->iov_base + src_seg_os;
	dst_addr = dst_iov_seg->iov_base + dst_seg_os;

	if (DEBUG_NI(ni, PTL_DBG_NI_06))
		p3_print("p3tcp_memcpy:"FMT_NIDPID": "FMT_PSZ_T" bytes from "
			 "%p -> %p\n", ni->nid, ni->pid,
			 copy_len, src_addr, dst_addr);

	memcpy(dst_addr, src_addr, copy_len);
	return PTL_OK;
}

static
int p3tcp_copy(lib_ni_t *ni,
	       api_mem_t *dst_addr, void *src_addr, ptl_size_t len)
{
	if (DEBUG_NI(ni, PTL_DBG_NI_06))
		p3_print("p3tcp_copy:"FMT_NIDPID": "FMT_PSZ_T" bytes from "
			 "%p -> %p\n", ni->nid, ni->pid,
			 len, src_addr, dst_addr);

	memcpy(dst_addr, src_addr, len);
	return 0;
}

static
int p3tcp_validate(lib_ni_t *ni,
		  api_mem_t *base, size_t extent, void **addrkey)
{
	return 0;
}

static
int p3tcp_vvalidate(lib_ni_t *ni,
		   ptl_md_iovec_t *iov, size_t iovlen, void **addrkey)
{
	return 0;
}

static
void p3tcp_invalidate(lib_ni_t *ni,
		     api_mem_t *base, size_t extent, void *addrkey) {}

static
void p3tcp_vinvalidate(lib_ni_t *ni,
		      ptl_md_iovec_t *iov, size_t iovlen, void *addrkey) {}

#endif /* !PTL_KERNEL_BLD */

static
int p3tcp_dist(lib_ni_t *ni, ptl_nid_t nid, unsigned long *dist)
{
	*dist = ni->nid == nid ? 0 : 1;
	return PTL_OK;
}

static
int p3tcp_set_debug(lib_ni_t *ni, unsigned int mask)
{
	p3tcp_data_t *d = ni->nal->private;
	d->debug = mask;
	return PTL_OK;
}

static
int p3tcp_progress(lib_ni_t *ni)
{
#if !defined USER_PROGRESS_THREAD && !defined PTL_KERNEL_BLD
	p3tcp_data_t *d = ni->nal->private;
	p3tcp_chan_poll(d, 1);	/* poll sockets once for progress */
#endif
	return PTL_OK;
}

static
lib_nal_t p3tcp_nal = {
	.ni = NULL,
	.nal_type = NULL,
	.private = NULL,
#ifdef PTL_KERNEL_BLD
	.msg_container_of = p3tcp_msg_container_of,
	.copy_to_api = p3lib_copy_to_api,
	.validate = p3lib_validate,
	.vvalidate = p3lib_vvalidate,
	.invalidate = p3lib_invalidate,
	.vinvalidate = p3lib_vinvalidate,
#else
	.copy_to_api = p3tcp_copy,
	.validate = p3tcp_validate,
	.vvalidate = p3tcp_vvalidate,
	.invalidate = p3tcp_invalidate,
	.vinvalidate = p3tcp_vinvalidate,
#endif
	.send = p3tcp_send,
	.recv = p3tcp_recv,
	.dequeue_msg = p3tcp_dequeue_msg,
	.mem_copy = p3tcp_memcpy,
	.dist = p3tcp_dist,
	.set_debug_flags = p3tcp_set_debug,
	.progress = p3tcp_progress
};

#ifdef PTL_KERNEL_BLD

static inline
int p3tcp_defport_setup(p3tcp_data_t *d)
{
	d->defport = p3tcp_def_portlist;
	d->ndport = p3tcp_ndport;
	return 0;
}

static inline
void *p3tcp_get_ifname(ptl_interface_t type)
{
	return p3tcp_if_name[p3tcp_if_idx(type)];
}

#else /* !PTL_KERNEL_BLD */

static
int p3tcp_portlist(p3tcp_data_t *d, const char *portstr,
		  unsigned **ports, unsigned *nports)
{
	unsigned i = 1;
	const char *ptr = portstr;

	*nports = 0;
	if (*ports)
		err_abort(EFAULT, "p3tcp_portlist: already allocated");

	while (*ptr) {
		if (i && *ptr == ':') ;
		else if (i) {
			i = 0;
			(*nports)++;
		}
		else if (*ptr == ':')
			i = 1;
		ptr++;
	}
	if (*nports <= 0)
		return -1;

	if (DEBUG_NI(d,PTL_DBG_NI_07))
		p3_print("Found port list with %d ports\n", *nports);
	i = 0;
	ptr = portstr;
	*ports = p3_malloc(*nports * sizeof(**ports));

	while (*ptr) {
		char *c;
		while (*ptr == ':')
			ptr++;
		(*ports)[i] = strtoul(ptr, &c, 0);
		if (DEBUG_NI(d,PTL_DBG_NI_07))
			p3_print("  port[%d] = %d\n", i, (*ports)[i]);
		if (++i == *nports)
			break;
		ptr = c;
	}
	if (i != *nports) {
		p3_print("Error reading port list\n");
		return -1;
	}
	return 0;
}

static
int p3tcp_portmap(p3tcp_data_t *d)
{
	FILE *f;
	char *fn;
	char *l, *p, buf[4096];
	int scan = 1, n;

	if (!(fn = getenv("PTL_PID2PORT_FILE"))) fn = "./map_pid2port";
	if (!(f = fopen(fn, "r"))) return 0;

parse:
	n = 0;
	fseek(f, 0, SEEK_SET);
	while ((l = fgets(buf, 4096, f))) {
		while (*l && isspace(*l))
			l++;	/* eat whitespace */
		if (!*l || *l == '#')
			continue;	/* ignore comments */
		if (!scan) {
			d->pid2pt[n].pp_pid = strtoul(l, &p, 0);
			d->pid2pt[n].pp_port = strtoul(p, NULL, 0);
			if (d->pid2pt[n].pp_port > MAX_PORT) {
				p3_print("read invalid port\n");
				return -1;
			}
		}
		n++;
	}
	if (scan) {
		d->npids = n;
		if (!(d->pid2pt = p3_malloc(n * sizeof(struct pid_pt)))) {
			p3_print("%s: allocating port array\n",
				 strerror(errno));
			return -1;
		}
		scan = 0;
		goto parse;
	}
	else {
		qsort(d->pid2pt,
		      d->npids, sizeof(struct pid_pt), map_cmp);
	}
	fclose(f);
	return 0;
}

/* Returns zero on success
 */
static inline
int p3tcp_defport_setup(p3tcp_data_t *d)
{
	int rc = 0;
	char *ev;

	if ((ev = getenv("PTL_DEF_PORTLIST")) &&
	    p3tcp_portlist(d, ev, &d->defport, &d->ndport)) {
		rc = 1;
	}
	else if (p3tcp_portmap(d))
		rc = 1;

	else {
		d->defport = p3tcp_def_portlist;
		d->ndport = p3tcp_ndport;
	}
	return rc;
}

static inline
void *p3tcp_get_ifname(ptl_interface_t type)
{
	char *str, *ev;

printf("%s():%d\n",__func__,__LINE__);
	if (type == PTL_NALTYPE_UTCP3) {
		str = "PTL_IFACE3";
	}
	else if (type == PTL_NALTYPE_UTCP2) {
		str = "PTL_IFACE2";
	}
	else if (type == PTL_NALTYPE_UTCP1) {
		str = "PTL_IFACE1";
	}
	else {
		if ((ev = getenv("PTL_IFACE0")))
			return ev;
		else
			str = "PTL_IFACE";
	}
	if (!(ev = getenv(str)))
		p3_print("Error: Must set %s\n", str);

	return ev;
}

#endif /* !PTL_KERNEL_BLD */

#ifdef USER_PROGRESS_THREAD
static pthread_t progress_thread;
#endif

int
p3tcp_init_private(ptl_interface_t type, const lib_ni_t *ni)
{
	p3tcp_data_t *nal_data;
	char *if_name;
	unsigned n;
printf("%s():%d\n",__func__,__LINE__);

#ifdef USER_PROGRESS_THREAD
	int flags, pipe_fd[2];

	if (pipe(pipe_fd))
		return -1;
#endif
	nal_data = &p3tcp_nal_data[p3tcp_if_idx(type)];

	if (nal_data->type != type)
		PTL_ROAD();
	nal_data->listen_fd = -1;
	nal_data->up = 1;
	p3lock_init(&nal_data->dlock);
#ifdef PTL_KERNEL_BLD
	init_MUTEX(&nal_data->poll_sem);
	nal_data->debug = p3tcp_debug;
#else
	nal_data->debug = ni->debug;
#endif
	INIT_LIST_HEAD(&nal_data->conn_wait);

	for (n = 0; n < TCP_FD_HASH_BINS; n++)
		INIT_LIST_HEAD(nal_data->chan_fd_hash + n);

	for (n = 0; n < TCP_NID_HASH_BINS; n++)
		INIT_LIST_HEAD(nal_data->chan_nid_hash + n);

	if (p3tcp_defport_setup(nal_data))
		return -1;

	if (!(if_name = p3tcp_get_ifname(type)))
		return -1;

	if (p3tcp_my_nid(if_name, &nal_data->nid))
		return -1;
#ifndef PTL_KERNEL_BLD
	nal_data->pid = ni->pid;
#endif
#ifdef USER_PROGRESS_THREAD
	pthread_mutex_init(&nal_data->shutdown_sync.lock, NULL);
	pthread_cond_init(&nal_data->shutdown_sync.wait, NULL);

	nal_data->rcv_wakeup = pipe_fd[0];
	nal_data->snd_wakeup = pipe_fd[1];

	flags = fcntl(nal_data->rcv_wakeup, F_GETFL);
	if (fcntl(nal_data->rcv_wakeup, F_SETFL, flags | O_NONBLOCK)) {
		p3_print("p3tcp_create_nal: fcntl: %s\n", strerror(errno));
		return -1;
	}

	if (pthread_create(&progress_thread, NULL, p3tcp_lib_start, nal_data)) {
		p3_print("PTL_IFACE_UTCP: pthread_create: %s",
			 strerror(errno));
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		return -1;
	}
#else
	if (p3tcp_lib_start(nal_data))
		return -1;
#endif
	return 0;
}

struct lib_nal *p3tcp_create_nal(ptl_interface_t type, const lib_ni_t *ni,
				 ptl_nid_t *nid, ptl_ni_limits_t *limits,
				 void *data, size_t data_sz)
{
printf("%s():%d\n",__func__,__LINE__);
	lib_nal_t *nal;

#ifndef PTL_KERNEL_BLD
	if (p3tcp_init_private(type, ni))
		return NULL;
#endif

	nal = p3_malloc(sizeof(lib_nal_t));
	if (!nal)
		return NULL;
	memset(nal, 0, sizeof(*nal));

	*nal = p3tcp_nal;
	nal->private = &p3tcp_nal_data[p3tcp_if_idx(type)];

	*nid = ((p3tcp_data_t *)(nal->private))->nid;
	*limits = lib_tcp_limits;

	return nal;
}

void p3tcp_stop_nal(lib_nal_t *nal)
{
	p3tcp_data_t *d;

	if (!nal)
		return;

	d = nal->private;

	if (DEBUG_NI(d,PTL_DBG_NI_04) ||
	    DEBUG_P3(d->debug, PTL_DBG_SHUTDOWN))
		p3_print("p3tcp_stop_nal: nal %p ni %p\n", nal, nal->ni);

	p3tcp_drain_chan(d, nal->ni);

#if defined USER_PROGRESS_THREAD
	{
		char c = 0;

		if (DEBUG_NI(d,PTL_DBG_NI_06))
			p3_print("p3tcp_destroy_nal: kill progress thread\n");

		if (write(d->snd_wakeup, &c, 1) <= 0)
			err_abort(EIO, "p3tcp_destroy_nal: send shutdown");

		pthread_mutex_lock(&d->shutdown_sync.lock);
		while (d->shutdown_sync.flag == 0) {
			pthread_cond_wait(&d->shutdown_sync.wait,
					  &d->shutdown_sync.lock);
		}
		pthread_mutex_unlock(&d->shutdown_sync.lock);

		/* wait for progress thread to exit */
		pthread_join(progress_thread, NULL);
	}
	close(d->snd_wakeup);
	close(d->rcv_wakeup);
#endif

#ifndef PTL_KERNEL_BLD
    p3tcp_libnal_shutdown(nal->private);

#endif

	if (DEBUG_NI(d,PTL_DBG_NI_04) ||
	    DEBUG_P3(d->debug, PTL_DBG_SHUTDOWN))
		p3_print("p3tcp_stop_nal: done nal %p.\n", nal);
}

void p3tcp_destroy_nal(lib_nal_t *nal)
{
	p3tcp_data_t *d;

	if (!nal)
		return;

	d = nal->private;

	if (DEBUG_NI(d,PTL_DBG_NI_04) ||
	    DEBUG_P3(d->debug, PTL_DBG_SHUTDOWN))
		p3_print("p3tcp_destroy_nal: nal %p ni %p\n", nal, nal->ni);

	nal->private = NULL;
	nal->ni = NULL;
	p3_free(nal);

	if (DEBUG_NI(d,PTL_DBG_NI_04) ||
	    DEBUG_P3(d->debug, PTL_DBG_SHUTDOWN))
		p3_print("p3tcp_destroy_nal: done nal %p.\n", nal);
}

#define TCP_WELL_KNOWN_PIDS 128

int p3tcp_pid_ranges(ptl_pid_t *first_ephemeral_pid,
		     ptl_pid_t *last_ephemeral_pid,
		     ptl_pid_t **well_known_pids, ptl_size_t *nwkpids)
{
	unsigned i;
	ptl_pid_t *p = p3_malloc(TCP_WELL_KNOWN_PIDS * sizeof(ptl_pid_t));

	if (!p) {
		*nwkpids = 0;
		*well_known_pids = p;
		return -ENOMEM;
	}
	*nwkpids = TCP_WELL_KNOWN_PIDS;
	for (i=0; i<TCP_WELL_KNOWN_PIDS; i++)
		p[i] = i;

	*well_known_pids = p;
	*first_ephemeral_pid = TCP_WELL_KNOWN_PIDS;
	*last_ephemeral_pid = (ptl_pid_t)-1;

	return 0;
}

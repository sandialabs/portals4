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
#ifndef _PTL3_P3LIB_P3VALIDATE_H_
#define _PTL3_P3LIB_P3VALIDATE_H_

/* p3validate provides a validate/invalidate service for kernel-space
 * Portals NALs.  Using this service, a NAL can pin user-space address
 * ranges into real memory, and obtain kernel-space descriptions of that
 * memory suitable for copying or hardware DMA command generation.
 */

#ifdef PTL_KERNEL_BLD
#ifndef NO_PINNED_PAGES

#include <asm/scatterlist.h>

struct p3_ubuf_map {
	unsigned long uaddr;		/* User-space addr of this segment */
	size_t length;			/* Length of this segment */
	size_t sglen;			/* Entries in sglist */
	struct scatterlist *sglist[1];	/* Variable size so last */
};

struct p3_addrmap {
	struct task_struct *task;
	volatile unsigned int wait;
	size_t maplen;			/* Entries in map */
	struct list_head msglist;	/* Queued messages using this map */
	struct p3_ubuf_map *map[1];	/* Variable size so last */
};

/* FIXME: the waitq doesn't belong here.  Since not all NALs will use
 * this validate service, we cannot rely on it (particularly invalidation)
 * to wait on message completion.  That functionality belongs in the
 * NAL implementation, in dequeue_msg().
 */
struct p3_addrmap_hook {
	struct p3_addrmap *volatile msg_addrmap;
	struct list_head msglist_link;	/* link into p3_addrmap msglist */
	wait_queue_head_t msg_waitq;	/* wait for this message to complete */
};

/* A NAL can use these to implement its validate/invalidate methods.
 */
extern int p3validate(struct p3_addrmap *am);
extern void p3invalidate(struct p3_addrmap *am);


/* Any NAL that uses this validate/invalidate service needs to use this to
 * declare a p3_addrmap_hook in their internal message-tracking struct.
 */
#define DECLARE_P3_ADDRMAP_HOOK struct p3_addrmap_hook am_hook

/* A NAL can use this to initialize the p3_addrmap_hook in its
 * message struct, as declared by the above.
 */
#define init_p3_addrmap_hook(container) \
do {								\
	(container)->am_hook.msg_addrmap = NULL;		\
	INIT_LIST_HEAD(&(container)->am_hook.msglist_link);	\
	init_waitqueue_head(&(container)->am_hook.msg_waitq);	\
} while (0)

/* A NAL can use this to get a pointer to the msg_addrmap in its
 * message struct, as declared by the above.
 */
#define P3_ADDRMAP_ADDRKEY(container) ((container)->am_hook.msg_addrmap)

/* A NAL can use this to get a pointer the NAL-internal message struct
 * containing a given p3_addrmap_hook pointer.  Hint: a NAL should use 
 * this to implement its nal->msg_containter_of() method.
 */
#define msg_containing_hook(msg_type,hook) \
	(container_of(hook, msg_type, am_hook))

/* A NAL should use this to attach the addrmap hook in its internal
 * message-tracking struct to the "void *addrkey" argument of the NAL
 * send/recv methods. "msg" is a pointer to the NAL-internal struct 
 * containing the hook.
 */
#define p3_addrmap_hook_add_ref(msg, addrkey) \
do {								\
	struct p3_addrmap_hook *hook = &(msg)->am_hook;		\
	hook->msg_addrmap = (struct p3_addrmap *)(addrkey);	\
	if ((addrkey))						\
		list_add_tail(&hook->msglist_link,		\
			      &hook->msg_addrmap->msglist);	\
} while (0)


/* A NAL should call this when it has completed send/receive operations
 * normally on a message.  "container" is a pointer to the NAL-internal struct
 * containing the hook.
 *
 * The only time this wakes anything up is when a program terminates
 * abnormally, and p3invalidate() tries to delete an addrkey for a message
 * being received.  In that case p3invalidate() sleeps while we finish the
 * message.
 */
#define p3_addrmap_hook_release(container) \
do {								\
	struct p3_addrmap_hook *hook = &(container)->am_hook;	\
	if (hook->msg_addrmap) {				\
		hook->msg_addrmap = NULL;			\
		list_del_init(&hook->msglist_link);		\
		wake_up(&hook->msg_waitq);			\
	}							\
} while (0)


/* A NAL should call this when it has to abort operations on a message.
 * For example, its dequeue_msg() method should call this before freeing
 * the internal structure used to track the message.
 *
 * "container" is a pointer to the NAL-internal struct containing the hook.
 */
#define p3_addrmap_hook_drop(container) \
do {								\
	struct p3_addrmap_hook *hook = &(container)->am_hook;	\
	if (hook->msg_addrmap) {				\
		hook->msg_addrmap = NULL;			\
		list_del_init(&hook->msglist_link);		\
	}							\
} while (0)


#else /* NO_PINNED_PAGES */

/* If we have a kernel-space library but we're not pinning pages,
 * we still use a non-NULL msg_addrmap in a hook as a flag that the
 * hook's container holds user-space addresses, so the NALs can know
 * to get a temporary kernel-space mapping if they need it. 
 * E.g., the TCP NAL.
 *
 * This is the value we use when an addrmap hook's container holds
 * user-space adresses, put we're not pinning pages.
 */
#define P3_MSG_ADDRMAP_NO_PIN ((void *)~0UL)

struct p3_ubuf_map {
	int pad;
};

struct p3_addrmap {
	int pad;
};

struct p3_addrmap_hook {
	struct p3_addrmap *msg_addrmap;
};

#define DECLARE_P3_ADDRMAP_HOOK  struct p3_addrmap_hook am_hook

#define init_p3_addrmap_hook(container) \
do {							\
	(container)->am_hook.msg_addrmap = NULL;	\
} while (0)

#define P3_ADDRMAP_ADDRKEY(container) ((container)->am_hook.msg_addrmap)

#define msg_containing_hook(msg_type,hook) \
	(container_of(hook, msg_type, am_hook))

#define p3_addrmap_hook_add_ref(container, addrkey) \
do {								\
	struct p3_addrmap_hook *hook = &(container)->am_hook;	\
	hook->msg_addrmap = (struct p3_addrmap *)(addrkey);	\
} while (0)

#define p3_addrmap_hook_release(container) \
do {							\
	(container)->am_hook.msg_addrmap = NULL;	\
} while (0)

#define p3_addrmap_hook_drop(container) \
do {							\
	(container)->am_hook.msg_addrmap = NULL;	\
} while (0)

#endif /* NO_PINNED_PAGES */

#else /* !PTL_KERNEL_BLD */

struct p3_ubuf_map {
	int pad;
};
struct p3_addrmap {
	int pad;
};

#define DECLARE_P3_ADDRMAP_HOOK 
#define init_p3_addrmap_hook(container) do {} while (0)
#define P3_ADDRMAP_ADDRKEY(container) (NULL)
#define msg_containing_hook(msg_type,hook) ((msg_type *)NULL)
#define p3_addrmap_hook_add_ref(container, addrkey) do {} while (0)
#define p3_addrmap_hook_release(container) do {} while (0)
#define p3_addrmap_hook_drop(container) do {} while (0)

#endif /* !PTL_KERNEL_BLD */

#endif /* _PTL3_P3LIB_P3VALIDATE_H_ */

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

#ifndef _PTL3_LIB_NAL_H_
#define _PTL3_LIB_NAL_H_

struct p3_addrmap_hook;

/* The library uses this to manage in-use PID values for a NAL type.
 *
 * We only track a small number of concurrent ephemeral PIDs; since we
 * are unlikely to have many concurrent Portals processes on a node using
 * a NAL type.
 */
#define MAX_EPIDS_IN_USE  64
#define BITS_IN_LONG (sizeof(long)*CHAR_BIT)
#define EPID_LONGS ((MAX_EPIDS_IN_USE + BITS_IN_LONG-1)/BITS_IN_LONG)

typedef struct lib_pids_inuse {

	/* Ephemeral PIDs
	 */
	unsigned long epid[EPID_LONGS];
	ptl_pid_t first_epid;
	ptl_pid_t last_epid;
	ptl_pid_t next_epid;
	unsigned epids_inuse_cnt;

	/* Well-known PIDs
	 */
	unsigned long *wkpid;
	ptl_pid_t *first_wkpid;
	ptl_pid_t *last_wkpid;
	ptl_pid_t *well_known_pids;
	ptl_size_t num_wkpids;

} lib_pids_inuse_t;

struct nal_type;
typedef struct nal_type nal_type_t;

/* Note: the NAL callback functions that return a value should return PTL_OK
 * on success; any other value signifies an error of one sort or another.
 */
typedef struct lib_nal {

	nal_type_t *nal_type;	/* We're an instance of this type */
	lib_ni_t *ni;		/* The interface this instance is supporting */
	void *private;		/* A NAL is free to use this as needed. */

	/* recv: Receives incoming message user data from a remote process. 
	 * The NAL should save lib_data away and use it when calling
	 * lib_finalize().  The value of nal_msg_data will be that given
	 * to the library in the lib_parse() call, which started processing
	 * for this message.  recv() can assume that *dst_iov will persist
	 * for as long as it takes to receive the message.
	 */
	int (*recv)(lib_ni_t *ni, unsigned long nal_msg_data, void *lib_data,
		    ptl_md_iovec_t *dst_iov, ptl_size_t iovlen,
		    ptl_size_t offset, ptl_size_t mlen, ptl_size_t rlen,
		    void *addrkey);

	/* send: Sends a preformatted header and user data to a specified
	 * remote process.  The NAL should save lib_data away and use it
	 * when calling lib_finalize().  When the library uses send() it is
	 * for a new message context which the NAL hasn't seen, so the NAL
	 * should set *nal_msg_data to a value that the library can use if
	 * it needs to request action from the NAL on that message context.
	 * This value must never be reused for a subsequent transaction.
	 * send() can assume that *src_iov will persist for as long as it
	 * takes to send the message.
	 */
	int (*send)(lib_ni_t *ni, unsigned long *nal_msg_data,
		    void *lib_data, ptl_process_id_t dst,
		    lib_mem_t *hdr, ptl_size_t hdrlen, 
		    ptl_md_iovec_t *src_iov, ptl_size_t iovlen,
		    ptl_size_t offset, ptl_size_t len,
		    void *addrkey);

	/* copy_to_api: Copies a block of data into a specified address in
	 * the API memory space.
	 *
	 * FIXME: we should really send along an addrkey for this.
	 */
	int (*copy_to_api)(lib_ni_t *ni,
			   api_mem_t *dst, void *src, ptl_size_t len);

	/* memcpy: copies a block of data, where both the source and
	 * destination may be in either library or API memory space.
	 * Note: all callers must guarantee that copy_len is short enough
	 * that the copied region does not cross into the next iov segment
	 * in either the source or destination iov args.
	 *
	 * FIXME: if we fix things so we validate all the API memory 
	 * that copy_to_api was used on, we can replace it with this,
	 * and fix the above fixme.
	 */
	int (*mem_copy)(lib_ni_t *ni,
			ptl_size_t copy_len,
			const ptl_md_iovec_t *src_iov_seg,
			ptl_size_t src_seg_os,
			ptl_size_t src_seg_idx,		/* which iov seg */
			void *src_addrkey,
			ptl_md_iovec_t *dst_iov_seg,
			ptl_size_t dst_seg_os,
			ptl_size_t dst_seg_idx,		/* which iov seg */
			void *dst_addrkey);		      

	/* Calculate a network "distance" to given node 
	 */
	int (*dist)(lib_ni_t *ni, ptl_nid_t nid, unsigned long *dist);

	/* Save the debug flags value
	 */
	int (*set_debug_flags)(lib_ni_t *ni, unsigned int mask);

	/* Try to make progress on outstanding messages 
	 */
	int (*progress)(lib_ni_t *ni);

	/* validate/invalidate: The library uses these to allow the NAL to
	 * prepare as necessary for data movement to/from an address range
	 * in the API memory space.
	 *
	 * WARNING: Once an addrkey has been passed to invalidate() or
	 * vinvalidate(), it is no longer valid, and must not be used again.
	 */
	int (*validate)(lib_ni_t *ni,
			api_mem_t *base, size_t extent, void **addrkey);

	int (*vvalidate)(lib_ni_t *ni,
			 ptl_md_iovec_t *iov, size_t iovlen, void **addrkey);

	void (*invalidate)(lib_ni_t *ni,
			   api_mem_t *base, size_t extent, void *addrkey);

	void (*vinvalidate)(lib_ni_t *ni,
			    ptl_md_iovec_t *iov, size_t iovlen, void *addrkey);

	/* Normally, the library wouldn't care about the details of NAL
	 * implementation.  However, the kernel-space p3lib_support provides a
	 * memory validate/invalidate service that NALs can use rather than
	 * implementing their own.  If your NAL is using this service read on;
	 * otherwise you don't need to implement the following functions.
	 *
	 * In order to support correct handling of exceptional circumstances
	 * such as abnormal termination, the validate/invalidate service needs
	 * to: 1) know that a NAL-internal message tracking object is using an
	 * address key; 2) retrieve message-specific information about the key
	 * used by a message; and 3) request the NAL not to start operations
	 * that have already been requested on a message via send()/recv().
	 *
	 * See kern/p3bvalidate.h for info on how to include the needed data
	 * in your NAL-internal message tracking object.  The validate service
	 * can retrieve it as necessary via the addrkey value.
	 *
	 * The validation service calls dequeue_msg() when it needs a NAL not
	 * to start operations that have already been requested on a message
	 * via send()/recv().
	 *
	 * dequeue_msg() should return PTL_OK if the NAL hasn't started
	 * processing the message, and can guarantee that it won't start, and
	 * has freed any internal resources it allocated via send()/recv(),
	 * and has called lib_finalize() for the message.
	 *
	 * Otherwise dequeue_msg() should return PTL_FAIL.
	 *
	 * Note that the validation service will wait for completion of any
	 * message for which dequeue_msg() does not return PTL_OK. 
	 *
	 * nal_internal_msg_struct is a pointer to the NAL-internal
	 * message tracking object.
	 */
	int (*dequeue_msg)(lib_ni_t *ni, void *nal_internal_msg_struct);
	void *(*msg_container_of)(lib_ni_t *ni, struct p3_addrmap_hook *hook);

} lib_nal_t;


/* A nal_create_t is the type of function that the library will call to
 * create a new instance of the NAL type.  It should set *nid and *limits
 * with values appropriate for the new NAL instance.  It should return
 * NULL on failure.
 */
typedef lib_nal_t *nal_create_t(ptl_interface_t type, const lib_ni_t *ni,
				ptl_nid_t *nid, ptl_ni_limits_t *limits,
				void *data, size_t data_sz);

/* A nal_stop_t is the type of function that the library will call to 
 * cause a NAL to stop moving data, prepatory to destroying it.  A NAL
 * that has a non-trivial memory validation service must ensure that
 * it is operational between calls to the nal_stop_t and nal_destroy_t
 * functions, so that memory descriptors can be unlinked.
 */
typedef void nal_stop_t(lib_nal_t *nal);

/* A nal_destroy_t is the type of function that the library will call to
 * destroy an existing instance of a NAL type.
 */
typedef void nal_destroy_t(lib_nal_t *nal);

/* A pid_ranges_t is the type of function that the library will call to
 * learn information on valid PID values for the NAL, so the library can
 * manage PIDs for the NAL type.
 *
 * The library assumes PID values are opaque, but can be ordered with < or >,
 * and incremented via ++.  When a process requests an ephemeral PID for an 
 * interface, the library will assign an unused one appropriate for the NAL
 * type from the closed range [<first_ephemeral_pid>, <last_ephemeral_pid>].
 * The NAL type can reserve a set of well-known values which can only be
 * assigned by specific request from the set <well_known_pids>, which must
 * be ordered by increasing value, so it can be searched via a binary search.
 *
 * The library will assume that a pid_ranges_t function will use p3_malloc
 * to allocate memory in *well_known_pids to hold num_wkpids well-known
 * PID values.  If num_wkpids is zero, *well_known_pids must be set NULL.
 *
 * Return value is a negative errno value.
 */
typedef int pid_ranges_t(ptl_pid_t *first_ephemeral_pid,
			 ptl_pid_t *last_ephemeral_pid,
			 ptl_pid_t **well_known_pids, ptl_size_t *num_wkpids);

/* A NAL type uses lib_register_nal(), which returns 0 if successful, to
 * register with the library that it is available for use.  
 */
int lib_register_nal(ptl_interface_t type, const char *name,
		     nal_create_t add, nal_stop_t stop, nal_destroy_t drop,
		     pid_ranges_t pids);

int lib_unregister_nal(ptl_interface_t type);


/* We use this type to manage registered NAL types.  
 */
struct nal_type {
	ptl_interface_t type;
	int refcnt;
	char *name;
	nal_create_t *create;
	nal_stop_t *stop;
	nal_destroy_t *destroy;
	lib_pids_inuse_t *pids_inuse;	
};

/* On the theory we shouldn't need to allow more NAL types than
 * we allow interfaces (arguable, I know), we'll have a static
 * NAL table with PTL_MAX_INTERFACES entries.
 *
 * If this ever proves to be a problem, we'll change it.
 */
extern nal_type_t p3nals[PTL_MAX_INTERFACES];

/* Call these versions with lib_update lock held
 */
static inline
nal_type_t *__get_nal_type(ptl_interface_t type)
{
	unsigned i;
	nal_type_t *nt = NULL;

	for (i=0; i<PTL_MAX_INTERFACES; i++)
		if (p3nals[i].type == type) {
			nt = &p3nals[i];
			nt->refcnt++;
			break;
		}
	return nt;
}

static inline
void __put_nal_type(nal_type_t *nt)
{
	nt->refcnt--;
}

static inline
nal_type_t *get_nal_type(ptl_interface_t type)
{
	nal_type_t *nt = NULL;

	p3_lock(&lib_update);
	nt = __get_nal_type(type);
	p3_unlock(&lib_update);
	return nt;
}

static inline
void put_nal_type(nal_type_t *nt)
{
	p3_lock(&lib_update);
	__put_nal_type(nt);
	p3_unlock(&lib_update);
}

#endif /* _PTL3_LIB_NAL_H_ */

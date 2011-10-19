/**
 * @file ptl_buf.c
 *
 * This file contains the implementation of
 * mr (memory region) class methods.
 */

#include "ptl_loc.h"

/**
 * Cleanup mr object.
 *
 * Called when the mr object is freed to the mr pool.
 *
 * @param[in] arg opaque reference to an mr object
 */
void mr_cleanup(void *arg)
{
	int err;
	mr_t *mr = (mr_t *)arg;

	if (mr->ibmr) {
		err = ibv_dereg_mr(mr->ibmr);
		if (err) {
			ptl_error("ibv_dereg_mr failed, ret = %d\n", err);
		}
		mr->ibmr = NULL;
	}

	if (mr->knem_cookie) {
		knem_unregister(mr->obj.obj_ni, mr->knem_cookie);
		mr->knem_cookie = 0;
	}
}

/**
 * Compare two mrs.
 *
 * mrs are sorted by starting address.
 *
 * @param[in] m1 first mr
 * @param[in] m2 second mr
 *
 * @return -1, 0, or +1 as m1 address is <, == or > m2 address
 */
static int mr_compare(struct mr *m1, struct mr *m2)
{
	return (m1->ibmr->addr < m2->ibmr->addr ? -1 :
		m1->ibmr->addr > m2->ibmr->addr);
}

/**
 * Generate RB tree internal functions.
 */
RB_GENERATE(the_root, mr, entry, mr_compare);

/**
 * Allocate and register a new memory region.
 *
 * For the new mr both an OFA verbs memory region and
 * a knem cookie are created.
 *
 * @param[in] ni from which to allocate mr
 * @param[in] start starting address of memory region
 * @param[in] length length of memory region
 * @param[out] mr_p address of return value
 *
 * @return status
 */ 
static int mr_create(ni_t *ni, void *start, ptl_size_t length, mr_t **mr_p)
{
	int err;
	mr_t *mr;
	void *end = start + length;
	struct ibv_mr *ibmr = NULL;
	int access;
	uint64_t knem_cookie = 0;

	start = (void *)((uintptr_t)start & ~((uintptr_t)pagesize - 1));
	end = (void *)(((uintptr_t)end + pagesize - 1) &
			~((uintptr_t)pagesize - 1));
	length = end - start;

	/*
	 * for now ask for everything
	 * TODO get more particular later
	 */
	access = IBV_ACCESS_LOCAL_WRITE
	       | IBV_ACCESS_REMOTE_WRITE
	       | IBV_ACCESS_REMOTE_READ
	       | IBV_ACCESS_REMOTE_ATOMIC;

	ibmr = ibv_reg_mr(ni->iface->pd, start, length, access);
	if (!ibmr) {
		WARN();
		err = PTL_FAIL;
		goto err1;
	}

	knem_cookie = knem_register(ni, start, length, PROT_READ | PROT_WRITE);
	if (!knem_cookie) {
		WARN();
		err = PTL_FAIL;
		goto err1;
	}

	err = mr_alloc(ni, &mr);
	if (err) {
		WARN();
		goto err1;
	}

	mr->ibmr = ibmr;
	mr->knem_cookie = knem_cookie;
	*mr_p = mr;

	return PTL_OK;

err1:
	if (ibmr)
		ibv_dereg_mr(ibmr);

	if (knem_cookie)
		knem_unregister(ni, knem_cookie);

	return err;
}

/**
 * Lookup an mr in the mr cache.
 *
 * Returns an mr satisfying the requested start/length. A new mr can
 * be allocated, or an existing one can be used. It is also possible that
 * one or more existing mrs will be merged into one.
 *
 * @param[in] ni in which to lookup range
 * @param[in] start starting address of memory range
 * @param[in] length length of range
 * @param[out] mr_p address of return value
 *
 * @return status
 */ 
int mr_lookup(ni_t *ni, void *start, ptl_size_t length, mr_t **mr_p)
{
	/*
	 * Search for an existing mr. The start address of the node must
	 * be less than or equal to the start address of the requested
	 * start. Find the closest start. 
	 */
	struct mr *link;
	struct mr *rb;
	struct mr *mr;
	struct mr *left_node;
	int ret;

	pthread_spin_lock(&ni->mr_tree_lock);

	link = RB_ROOT(&ni->mr_tree);
	left_node = NULL;

	mr = NULL;

	while (link) {
		mr = link;

		if (start < mr->ibmr->addr)
			link = RB_LEFT(mr, entry);
		else {
			if (mr->ibmr->addr+mr->ibmr->length >= start+length) {
				/* Requested mr fits in an existing region. */
				mr_get(mr);
				ret = 0;
				*mr_p = mr;
				goto done;
			}
			left_node = mr;
			link = RB_RIGHT(mr, entry);
		}
	}

	mr = NULL;

	/* Extend region to the left. */
	if (left_node &&
		(start <= (left_node->ibmr->addr + left_node->ibmr->length))) {
			length += start - left_node->ibmr->addr;
			start = left_node->ibmr->addr;

			/* First merge node. Will be replaced later. */
			mr = left_node;
	}

	/* Extend the region to the right. */
	if (left_node)
		rb = RB_NEXT(the_root, &ni->mr_tree, left_node);
	else
		rb = RB_MIN(the_root, &ni->mr_tree);
	while (rb) {
		struct mr *next_rb = RB_NEXT(the_root, &ni->mr_tree, rb);

		/* Check whether new region can be merged with this node. */
		if (start+length >= rb->ibmr->addr) {
			/* Is it completely part of the new region ? */
			size_t new_length = rb->ibmr->addr +
				rb->ibmr->length - start;
			if (new_length > length)
				length = new_length;

			if (mr) {
				/* Remove the node since it will be included
				 * in the new mr. */
				RB_REMOVE(the_root, &ni->mr_tree, rb);
				mr_put(rb);
			} else {
				/* First merge node. Will be replaced later. */
				mr = rb;
			}
		} else {
			break;
		}

		rb = next_rb;
	}

	if (mr) {
		/* Remove included mr on the right. */
		RB_REMOVE(the_root, &ni->mr_tree, mr);
		mr_put(mr);
		mr = NULL;
	}

	/* Insert the new node */
	ret = mr_create(ni, start, length, mr_p);
	if (ret) {
		/* That's not going to be good since we may have removed some
		 * regions. However that case should not happen. */
		WARN();
	} else {
		void *res;

		mr = *mr_p;
		mr_get(mr);

		res = RB_INSERT(the_root, &ni->mr_tree, mr);
		assert(res == NULL);	/* should never happen */
	}

 done:
	pthread_spin_unlock(&ni->mr_tree_lock);

	return ret;
}

/**
 * Empty the mr cache.
 *
 * @param[in] ni for which cache is emptied
 */
void cleanup_mr_tree(ni_t *ni)
{
	mr_t *mr;
	mr_t *next_mr;

	pthread_spin_lock(&ni->mr_tree_lock);

	for (mr = RB_MIN(the_root, &ni->mr_tree); mr != NULL; mr = next_mr) {
		next_mr = RB_NEXT(the_root, &ni->mr_tree, mr);
		RB_REMOVE(the_root, &ni->mr_tree, mr);
		mr_put(mr);
	}

	pthread_spin_unlock(&ni->mr_tree_lock);
}

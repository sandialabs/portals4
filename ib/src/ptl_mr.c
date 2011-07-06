/*
 * ptl_mr.c
 */

#include "ptl_loc.h"

void mr_release(void *arg)
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
}

/* Order the MRs in the tree by start address. */
int mr_compare(struct mr *m1, struct mr *m2)
{
	return (m1->ibmr->addr < m2->ibmr->addr ? -1 : m1->ibmr->addr > m2->ibmr->addr);
}

/* Generate RB tree internal functions. */
RB_GENERATE(the_root, mr, entry, mr_compare);

/* Allocate and register a new memory region. */
static int mr_create(ni_t *ni, void *start, ptl_size_t length, mr_t **mr_p)
{
	int err;
	mr_t *mr;
	void *end = start + length;
	struct ibv_mr *ibmr;
	int access;

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

	err = mr_alloc(ni, &mr);
	if (err) {
		WARN();
		goto err1;
	}

	mr->ibmr = ibmr;
	*mr_p = mr;

	return PTL_OK;

err1:
	return err;
}

/* Returns an MR satisfying the requested start/length. A new MR can
 * be allocated, or an existing one can be used. It is also possible that
 * one or more existing MRs will be merged into one. */
int mr_lookup(ni_t *ni, void *start, ptl_size_t length, mr_t **mr_p)
{
	/*
	 * Search for an existing MR. The start address of the node must
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
				/* Requested MR fits in an existing region. */
				mr_ref(mr);
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
			size_t new_length = rb->ibmr->addr + rb->ibmr->length - start;
			if (new_length > length)
				length = new_length;

			if (mr) {
				/* Remove the node since it will be included in the
				 * new MR. */
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
		/* Remove included MR on the right. */
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
		mr_ref(mr);

		res = RB_INSERT(the_root, &ni->mr_tree, mr);
		assert(res == NULL);			/* should never happen */
	}

 done:
	pthread_spin_unlock(&ni->mr_tree_lock);

	return ret;
}

/* Empty the tree of MRs. Called when the NI shuts down. */
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

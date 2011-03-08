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

	ibmr = ibv_reg_mr(ni->pd, start, length, access);
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

	/* For now do not drop mr's take one more reference */
	mr_ref(mr);

	mr->start = start;
	mr->length = length;

	pthread_spin_lock(&ni->mr_list_lock);
	list_add(&mr->list, &ni->mr_list);
	pthread_spin_unlock(&ni->mr_list_lock);

	*mr_p = mr;
	return PTL_OK;

err1:
	return err;
}

/*
 * mr_lookup
 *	TODO replace linear search with something better
 *	this is a placeholder
 */
int mr_lookup(ni_t *ni, void *start, ptl_size_t length, mr_t **mr_p)
{
	mr_t *mr;
	struct list_head *l;

	if (debug)
		printf("mr_lookup: start = %p, length = %" PRIu64 "\n",
			start, length);

	pthread_spin_lock(&ni->mr_list_lock);
	list_for_each(l, &ni->mr_list) {
		mr = list_entry(l, mr_t , list);
		if ((mr->start <= start) &&
		    ((mr->start + mr->length) >= (start + length))) {
			mr_ref(mr);
			pthread_spin_unlock(&ni->mr_list_lock);
			goto found;
		}
	}
	pthread_spin_unlock(&ni->mr_list_lock);

	if (debug) printf("creating a new mr\n");
	return mr_create(ni, start, length, mr_p);

found:
	if (debug) printf("found an existing mr\n");
	*mr_p = mr;
	return PTL_OK;
}

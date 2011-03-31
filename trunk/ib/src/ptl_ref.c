/*
 * ptl_ref.c (modeled roughly on linux kref.c)
 */

#include "ptl_loc.h"

void ref_set(struct ref *ref, int num)
{
	ref->ref_cnt = num;
}

void ref_init(struct ref *ref)
{
	ref_set(ref, 1);
}

void ref_get(struct ref *ref)
{
	int ref_cnt;

	ref_cnt = __sync_fetch_and_add(&ref->ref_cnt, 1);

	if (ref_cnt <= 0)
		ptl_warn("ref_cnt = %d <= 0", ref->ref_cnt);
}

int ref_put(struct ref *ref, void (*release)(ref_t *ref))
{
	int ref_cnt;

	ref_cnt = __sync_sub_and_fetch(&ref->ref_cnt, 1);

	if (ref_cnt == 0) {
	        release(ref);
	        return 1;
	}

	return 0;
}

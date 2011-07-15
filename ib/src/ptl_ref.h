/*
 * ptl_ref.h
 */

#ifndef PTL_REF_H
#define PTL_REF_H

typedef struct ref {
	int			ref_cnt;
} ref_t;

static inline void ref_set(struct ref *ref, int num)
{
	ref->ref_cnt = num;
}

static inline void ref_init(struct ref *ref)
{
	ref_set(ref, 1);
}

static inline void ref_get(struct ref *ref)
{
	int ref_cnt;

	ref_cnt = __sync_fetch_and_add(&ref->ref_cnt, 1);

	assert(ref_cnt >= 1);
}

static inline int ref_put(struct ref *ref, void (*release)(ref_t *ref))
{
	int ref_cnt;

	ref_cnt = __sync_sub_and_fetch(&ref->ref_cnt, 1);

	if (ref_cnt == 0) {
	        release(ref);
	        return 1;
	}

	return 0;
}

#endif /* PTL_REF_H */

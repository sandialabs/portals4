/*
 * ptl_ref.h
 */

#ifndef PTL_REF_H
#define PTL_REF_H

typedef struct ref {
	int			ref_cnt;
} ref_t;

void ref_set(struct ref *ref, int num);
void ref_init(struct ref *ref);
void ref_get(struct ref *ref);
int ref_put(struct ref *ref, void (*release)(ref_t *ref));

#endif /* PTL_REF_H */

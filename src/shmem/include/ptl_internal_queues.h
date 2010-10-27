#ifndef PTL_INTERNAL_QUEUES_H
#define PTL_INTERNAL_QUEUES_H

typedef struct ptl_internal_qnode_s {
    void *value;
    volatile struct ptl_internal_qnode_s *volatile next;
} ptl_internal_qnode_t;

typedef struct {
    volatile ptl_internal_qnode_t *volatile head;
    volatile ptl_internal_qnode_t *volatile tail;
} ptl_internal_q_t;

void PtlInternalQueueInit(
    ptl_internal_q_t * q);
void PtlInternalQueueDestroy(
    ptl_internal_q_t * q);
void PtlInternalQueueAppend(
    ptl_internal_q_t * q,
    void *t);
void *PtlInternalQueuePop(
    ptl_internal_q_t * q);


#endif
/* vim:set expandtab: */

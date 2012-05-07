#ifndef PTL_INTERNAL_QUEUES_H
#define PTL_INTERNAL_QUEUES_H

typedef struct ptl_internal_qnode_s {
    void                        *value;
    struct ptl_internal_qnode_s *next;
} ptl_internal_qnode_t;

typedef struct {
    ptl_internal_qnode_t *head;
    ptl_internal_qnode_t *tail;
} ptl_internal_q_t;

void PtlInternalQueueInit(ptl_internal_q_t *q);
void PtlInternalQueueDestroy(ptl_internal_q_t *q);
void PtlInternalQueueAppend(ptl_internal_q_t *q,
                            void             *t);
void *PtlInternalQueuePop(ptl_internal_q_t *q);

#endif /* ifndef PTL_INTERNAL_QUEUES_H */
/* vim:set expandtab: */

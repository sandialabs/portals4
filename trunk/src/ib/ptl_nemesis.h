#ifndef PTL_INTERNAL_NEMESIS_H
#define PTL_INTERNAL_NEMESIS_H

#define CACHELINE_WIDTH 64

/* head, tail and shadow_head are offsets into the comm_pad. */
typedef struct {
    /* The First Cacheline */
	unsigned long  head;
    unsigned long  tail;
    uint8_t        pad1[CACHELINE_WIDTH - (2 * sizeof(unsigned long))];
    /* The Second Cacheline */
    unsigned long  shadow_head;
    uint8_t        pad2[CACHELINE_WIDTH - sizeof(unsigned long)];
} NEMESIS_queue ALIGNED (CACHELINE_WIDTH);

typedef struct NEMESIS_blocking_queue {
    NEMESIS_queue q;
    volatile uint32_t frustration;
    pthread_cond_t    trigger;
    pthread_mutex_t   trigger_lock;
} NEMESIS_blocking_queue;

void PtlInternalFragmentSetup(ni_t *ni);
void PtlInternalFragmentToss(ni_t *ni, buf_t *buf, ptl_pid_t dest);
buf_t *PtlInternalFragmentReceive(ni_t *ni);

#endif /* ifndef PTL_INTERNAL_NEMESIS_H */

/* vim:set expandtab: */

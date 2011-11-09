/**
 * @file ptl_queue.h
 */

#ifndef PTL_QUEUE_H
#define PTL_QUEUE_H

#define CACHELINE_WIDTH 64

/**
 * @brief shared memory buffer queue
 */
struct queue {
	/* The First Cacheline */
	unsigned long		head;
	unsigned long		tail;
	uint8_t			pad1[CACHELINE_WIDTH -
					(2*sizeof(unsigned long))];
	/* The Second Cacheline */
	unsigned long		shadow_head;
	uint8_t			pad2[CACHELINE_WIDTH -
					sizeof(unsigned long)];
	atomic_t		frustration;
};

typedef struct queue queue_t;

void queue_init(ni_t *ni);

void shmem_enqueue(ni_t *ni, buf_t *buf, ptl_pid_t dest);

buf_t *shmem_dequeue(ni_t *ni);

#endif /* PTL_QUEUE_H */

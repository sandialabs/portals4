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
};

typedef struct queue queue_t;

void queue_init(queue_t *queue);
void enqueue(const void *comm_pad, queue_t *restrict queue, buf_t *buf);
buf_t *dequeue(const void *comm_pad, queue_t *queue);


#endif /* PTL_QUEUE_H */

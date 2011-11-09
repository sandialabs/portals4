/**
 * @file ptl_queue.c
 */

#include "ptl_loc.h"

/** Convert an offset in shared memory buffer to buf */
#define OFF2PTR(ni, off) (((off) == 0) ? NULL : \
	(buf_t *)((unsigned char *)(ni)->shmem.comm_pad + (off)))

/** convert a buf pointer to an offset in shared memory region */
#define PTR2OFF(ni, ptr) (((ptr) == NULL) ? 0 : \
	((unsigned char *)(ptr) - (unsigned char *)(ni)->shmem.comm_pad))

/**
 * @brief enqueue a buf on a queue.
 *
 * @param[in] ni the network interface.
 * @param[in] queue the queue.
 * @param[in] buf the buf.
 */
static void enqueue(ni_t *ni, queue_t *restrict queue, buf_t *buf)
{
	unsigned long off;
	unsigned long off_prev;

	off = PTR2OFF(ni, buf);
	off_prev = (uintptr_t)atomic_swap_ptr(
				(void **)(uintptr_t)&(queue->tail),
				(void *)(uintptr_t)off);

	if (off_prev == 0)
		queue->head = off;
	else
		OFF2PTR(ni, off_prev)->obj.next = (void *)off;
}

/**
 * @brief dequeue a buf from a shared memory queue.
 *
 * @param[in] ni the network interface.
 * @param[in] queue the queue.
 *
 * @return a buf.
 */
static buf_t *dequeue(ni_t *ni, queue_t *queue)
{
	buf_t *buf;

	if (!queue->shadow_head) {
		if (!queue->head)
			return NULL;
		queue->shadow_head = queue->head;
		queue->head	= 0;
	}

	buf = OFF2PTR(ni, queue->shadow_head);

	if (buf) {
		if (buf->obj.next) {
			queue->shadow_head = (uintptr_t)buf->obj.next;
			buf->obj.next = NULL;
		} else {
			unsigned long old;

			queue->shadow_head = 0;
			old = (uintptr_t)__sync_val_compare_and_swap(
				&(queue->tail), PTR2OFF(ni, buf), 0);

			if (old != PTR2OFF(ni,buf)) {
				while (buf->obj.next == NULL)
					SPINLOCK_BODY();

				queue->shadow_head = (uintptr_t)buf->obj.next;
				buf->obj.next = NULL;
			}
		}
	}

	return buf;
}

/**
 * @brief Initialize a queue.
 *
 * @param[in] ni 
 */
void queue_init(ni_t *ni)
{
	queue_t *queue = (queue_t *)(ni->shmem.comm_pad + pagesize +
			(ni->shmem.per_proc_comm_buf_size*ni->shmem.index));

	queue->head = 0;
	queue->tail = 0;
	queue->shadow_head = 0;

	ni->shmem.queue = queue;
}

/**
 * @brief enqueue a buf to a pid using shared memory.
 *
 * @param[in] ni the network interface
 * @param[in] buf the buf
 * @param[in] dest the destination pid
 */
void shmem_enqueue(ni_t *ni, buf_t *buf, ptl_pid_t dest)
{	      
	queue_t *queue = (queue_t *)(ni->shmem.comm_pad + pagesize +
			   (ni->shmem.per_proc_comm_buf_size * dest));

	buf->obj.next = NULL;
	enqueue(ni, queue, buf);
}

/**
 * @brief dequeue a buf using shared memory.
 *
 * @param[in] ni the network interface.
 */
buf_t *shmem_dequeue(ni_t *ni)
{			       
	return dequeue(ni, ni->shmem.queue);
}

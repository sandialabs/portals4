/**
 * @file ptl_queue.c
 */

#include "ptl_loc.h"

/** Convert an offset in shared memory buffer to buf */
#define OFF2PTR(commpad, off) (((off) == 0) ? NULL : \
       (buf_t *)((unsigned char *)(commpad) + (off)))

/** convert a buf pointer to an offset in shared memory region */
#define PTR2OFF(commpad, ptr) ((void *)(ptr) - (commpad))

/**
 * @brief enqueue a buf on a queue.
 *
 * @param[in] queue the queue.
 * @param[in] buf the buf.
 */
void enqueue(const void *comm_pad, queue_t *restrict queue, buf_t *buf)
{
	unsigned long off;
	unsigned long off_prev;

	off = PTR2OFF(comm_pad, buf);
	off_prev = (uintptr_t)atomic_swap_ptr(
				(void **)(uintptr_t)&(queue->tail),
				(void *)(uintptr_t)off);

	if (off_prev == 0)
		queue->head = off;
	else
		OFF2PTR(comm_pad, off_prev)->obj.next = (void *)off;
}

/**
 * @brief dequeue a buf from a shared memory queue.
 *
 * @param[in] queue the queue.
 *
 * @return a buf.
 */
buf_t *dequeue(const void *comm_pad, queue_t *queue)
{
	buf_t *buf;

	if (!queue->shadow_head) {
		if (!queue->head)
			return NULL;
		queue->shadow_head = queue->head;
		queue->head	= 0;
	}

	buf = OFF2PTR(comm_pad, queue->shadow_head);

	if (buf) {
		if (buf->obj.next) {
			queue->shadow_head = (uintptr_t)buf->obj.next;
			buf->obj.next = NULL;
		} else {
			unsigned long old;

			queue->shadow_head = 0;
			old = (uintptr_t)__sync_val_compare_and_swap(
				&(queue->tail), PTR2OFF(comm_pad, buf), 0);

			if (old != PTR2OFF(comm_pad, buf)) {
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
 * @param[in] pointer to the queue to initialize
 */
void queue_init(queue_t *queue)
{
	queue->head = 0;
	queue->tail = 0;
	queue->shadow_head = 0;
}

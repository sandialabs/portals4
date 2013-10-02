/**
 * @file ptl_queue.c
 */

#include "ptl_loc.h"

/** Convert an offset in shared memory buffer to buf */
#define OFF2PTR(commpad, off) (((off) == 0) ? NULL : \
       (obj_t *)((unsigned char *)(commpad) + (off)))

/** convert a buf pointer to an offset in shared memory region */
#define PTR2OFF(commpad, ptr) ((void *)(ptr) - (commpad))

/**
 * @brief enqueue a buf on a queue.
 *
 * @param[in] queue the queue.
 * @param[in] obj the object to enqueue. obj->next MUST be NULL.
 */
void enqueue(const void *comm_pad, queue_t *restrict queue, obj_t *obj)
{
    unsigned long off;
    unsigned long off_prev;

    off = PTR2OFF(comm_pad, obj);
    off_prev =
        (uintptr_t) atomic_swap_ptr((void **)(uintptr_t) & (queue->tail),
                                    (void *)(uintptr_t) off);

    if (off_prev == 0)
        queue->head = off;
    else
        OFF2PTR(comm_pad, off_prev)->next = (void *)off;
}

/**
 * @brief dequeue a buf from a shared memory queue.
 *
 * @param[in] queue the queue.
 *
 * @return an object.
 */
obj_t *dequeue(const void *comm_pad, queue_t *queue)
{
    obj_t *obj;

    if (!queue->shadow_head) {
        if (!queue->head)
            return NULL;
        queue->shadow_head = queue->head;
        queue->head = 0;
    }

    obj = OFF2PTR(comm_pad, queue->shadow_head);

    if (obj) {
        if (obj->next) {
            queue->shadow_head = (uintptr_t) obj->next;
            obj->next = NULL;
        } else {
            unsigned long old;

            queue->shadow_head = 0;
            old =
                (uintptr_t) __sync_val_compare_and_swap(&(queue->tail),
                                                        PTR2OFF(comm_pad,
                                                                obj), 0);

            if (old != PTR2OFF(comm_pad, obj)) {
                while (obj->next == NULL)
                    SPINLOCK_BODY();

                queue->shadow_head = (uintptr_t) obj->next;
                obj->next = NULL;
            }
        }
    }

    return obj;
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

#ifndef MC_COMMAND_QUEUE_H
#define MC_COMMAND_QUEUE_H

#include <stddef.h>

/* 
 * A command queue entry
 */
struct ptl_cqe_t;
typedef struct ptl_cqe_t ptl_cqe_t;


/* 
 * Endpoint information suitable for attaching the other process to
 * the first half of the command queue entry.  The contents are
 * implementation defined.
 */
struct ptl_cq_info_t;
typedef struct ptl_cq_info_t ptl_cq_info_t;


/*
 * Command queue structure
 */
struct ptl_cq_t;
typedef struct ptl_cq_t ptl_cq_t;


#include "command_queue_xpmem.h"

/*
 * ptl_cq_create
 *
 * Create a command queue
 *
 * Arguments:
 *  entry_size (IN)  - size of command queue entry
 *  cq (OUT)         - completion queue handle
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 */
int ptl_cq_create(size_t entry_size, int num_entries, struct ptl_cq_t *cq);


/*
 * ptl_cq_info_get
 *
 * Get information for attaching to an existing command queue
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 */
int ptl_cq_info_get(ptl_cq_info_t *info);


/*
 * ptl_cq_attach
 *
 * Attach local cq to remote cq.
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 *  This is a remote, blocking operation.  Both sides must call attach
 *  (with endpoint information from the other side) for attach to
 *  succeed.
 */
int ptl_cq_attach(struct ptl_cq_info_t *, ptl_cq_t *cq);


/*
 * ptl_cq_destroy
 *
 * Destroy a command queue
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 *  This is a local operation, but the status of a remote send / recv
 *  operation is undefined once the local process has called destroy.
 */
int ptl_cq_destroy(struct ptl_cq_info_t *endpoint);


/*
 * ptl_cq_entry_alloc
 *
 * Allocate a new command queue entry buffer
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 */
int ptl_cq_entry_alloc(ptl_cq_t *cq, ptl_cqe_t** entry);


/*
 * ptl_cq_entry_free
 *
 * return a command queue entry buffer to the originator's free list.
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 */
int ptl_cq_entry_free(ptl_cq_t *cq, ptl_cqe_t* entry);


/*
 * ptl_cq_entry_send
 *
 * send a command queue entry to the remote peer
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 *  ptl_cq_entry_send transfers ownership of the cqe buffer from the
 *  caller to the implementation.  The caller does *NOT* need to call
 *  ptl_cq_entry_free on the buffer.
 */
int ptl_cq_entry_send(ptl_cq_t *cq, ptl_cqe_t *entry);


/*
 * ptl_cq_entry_send
 *
 * send a command queue entry to the remote peer
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 *  ptl_cq_entry_recv transfers ownership of the cqe buffer from the
 *  implementation to the caller.  The caller must later call
 *  ptl_cq_entry_send or ptl_cq_entry_free to transfer ownership back
 *  to the implementation or original allocator.
 */
int ptl_cq_entry_recv(ptl_cq_t *cq, ptl_cqe_t *entry);

#endif

#ifndef MC_COMMAND_QUEUE_H
#define MC_COMMAND_QUEUE_H

#include <stddef.h>

/* 
 * A user-defined command queue entry.  User must specify size of
 * command queue entry in ptl_cq_create.
 */
union ptl_cqe_t;
typedef union ptl_cqe_t ptl_cqe_t;


/* 
 * Endpoint information suitable for attaching the other process to
 * the first half of the command queue entry.  The contents and size
 * are implementation defined.
 */
struct ptl_cq_info_t;
typedef struct ptl_cq_info_t ptl_cq_info_t;


/*
 * Opaque command queue structure.
 */
struct ptl_cq_t;
typedef struct ptl_cq_t* ptl_cq_handle_t;


/*
 * ptl_cq_create
 *
 * Create the local portion of a command queue.
 *
 * Arguments:
 *  entry_size (IN)      - size of command queue entry
 *  send_queue_size (IN) - Number of local command queue entries 
 *                         (number of local messages that can be
 *                         outstanding)
 *  recv_queue_sie (IN)  - Number of commands that can be queued
 *                         in local receive queue
 *  my_index (IN)        - Local process index.  Must be a small
 *                         integer, unique to any attached process.
 *  cq_h (OUT)           - completion queue handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 */
int ptl_cq_create(size_t entry_size, 
                  int send_queue_size, int recv_queue_size,
                  int my_index, ptl_cq_handle_t *cq_h);


/*
 * ptl_cq_info_get
 *
 * Get local information to allow remote process to attach to queue
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *  info (OUT)           - pointer to an (opaque) info structure 
 *  info_len (OUT)       - pointer to size_t variable which will
 *                         specify length of info
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  If ptl_cq_info_get returns 0, info must be freed by the caller.
 */
int ptl_cq_info_get(ptl_cq_handle_t cq_h, ptl_cq_info_t **info, size_t *info_len);


/*
 * ptl_cq_attach
 *
 * Attach local cq to remote cq.
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *  info (IN)            - pointer to opaque info structure from remote process
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  This is a remote, possibly blocking operation.  Both sides must
 *  call attach (with endpoint information from the other side) for
 *  attach to succeed.  It is possible to attach multiple remote cqs
 *  into a single local cq.
 */
int ptl_cq_attach(ptl_cq_handle_t cq_h, ptl_cq_info_t *info);


/*
 * ptl_cq_destroy
 *
 * Destroy a command queue
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  This is a local operation, but the status of a remote send / recv
 *  operation is undefined once the local process has called destroy.
 */
int ptl_cq_destroy(ptl_cq_handle_t cq_h);


/*
 * ptl_cq_entry_alloc
 *
 * Allocate a new command queue entry buffer
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *  entry (OUT)          - completion queue entry pointer
 *
 * Return values:
 *  0                    - success
 *  1                    - no fragments available
 *  -1                   - other failure (errno will be set)
 *
 * Notes:
 */
int ptl_cq_entry_alloc(ptl_cq_handle_t cq_h, ptl_cqe_t** entry);


/*
 * ptl_cq_entry_free
 *
 * return a command queue entry buffer to the originator's free list.
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *  entry (IN)           - completion queue entry
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  In most cases, this function is not necessary.  An entry is
 *  returned to the implementation as part of the send call.  This
 *  call may be useful when an error is found between alloc and send
 *  in user code
 */
int ptl_cq_entry_free(ptl_cq_handle_t cq_h, ptl_cqe_t* entry);


/*
 * ptl_cq_entry_send
 *
 * send a command queue entry to the remote peer
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *  index (IN)           - peer index message target
 *  entry (IN)           - completion queue entry
 *  len (IN)             - length of completion queue entry.  Must
 *                         be no bigger than the max entry size
 *                         passed to alloc.
 *
 * Return values:
 *  0                    - success
 *  1                    - no receive slots available at index
 *  -1                   - other failure (errno will be set)
 *
 * Notes:
 *  ptl_cq_entry_send transfers ownership of the cqe buffer from the
 *  caller to the implementation.  The caller should not call
 *  ptl_cq_entry_free on the buffer.
 */
int ptl_cq_entry_send(ptl_cq_handle_t cq_h, int index, 
                      ptl_cqe_t *entry, size_t len);


/*
 * ptl_cq_entry_send_block
 *
 * send a command queue entry to the remote peer
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *  index (IN)           - peer index message target
 *  entry (IN)           - completion queue entry
 *  len (IN)             - length of completion queue entry.  Must
 *                         be no bigger than the max entry size
 *                         passed to alloc.
 *
 * Return values:
 *  0                    - success
 *  1                    - no receive slots available at index
 *  -1                   - other failure (errno will be set)
 *
 * Notes:
 *  ptl_cq_entry_send transfers ownership of the cqe buffer from the
 *  caller to the implementation.  The caller should not call
 *  ptl_cq_entry_free on the buffer.
 */
int ptl_cq_entry_send_block(ptl_cq_handle_t cq_h, int index, 
                            ptl_cqe_t *entry, size_t len);


/*
 * ptl_cq_entry_recv
 *
 * send a command queue entry to the remote peer
 *
 * Arguments:
 *  cq_h (IN)            - completion queue handle
 *  entry (IN/OUT)       - completion queue entry
 *
 * Return values:
 *  0                    - success
 *  1                    - no messages available
 *  -1                   - other failure (errno will be set)
 *
 * Notes:
 *  The user must provide a buffer of the appropriate size.
 */
int ptl_cq_entry_recv(ptl_cq_handle_t cq_h, ptl_cqe_t *entry);

#endif

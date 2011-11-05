#ifndef MC_COMMAND_QUEUE_H
#define MC_COMMAND_QUEUE_H

/* 
 * A command queue entry, defined in command_queue_entry.h
 */
struct ptl_cqe_t;
typedef struct ptl_cqe_t ptl_cqe_t;


/* 
 * Endpoint information suitable for attaching the other process to
 * the first half of the command queue entry.  The contents and size
 * are implementation defined.
 */
struct ptl_cq_info_t;
typedef struct ptl_cq_info_t ptl_cq_info_t;


/*
 * Command queue structure
 */
struct ptl_cq_t;
typedef struct ptl_cq_t* ptl_cq_handle_t;


/*
 * ptl_cq_create
 *
 * Create a command queue
 *
 * Arguments:
 *  entry_size (IN)  - size of command queue entry
 *  num_entries (IN) - number of entries in receive queue
 *  cq (OUT)         - completion queue handle
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 */
int ptl_cq_create(size_t entry_size, int num_entries, 
                  int my_index, ptl_cq_handle_t *cq);


/*
 * ptl_cq_info_get
 *
 * Get local information to allow remote process to attach to queue
 *
 * Arguments:
 *  cq (IN)        - completion queue handle
 *  info (OUT)     - pointer to an (opaque) info structure 
 *  info_len (OUT) - pointer to size_t variable which will
 *                   specify length of info
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 *  If ptl_cq_info_get returns 0, *info must be freed by the caller.
 */
int ptl_cq_info_get(ptl_cq_handle_t cq, ptl_cq_info_t **info, size_t *info_len);


/*
 * ptl_cq_attach
 *
 * Attach local cq to remote cq.
 *
 * Arguments:
 *  cq (IN)        - completion queue handle
 *  info (IN)      - pointer to opaque info structure from remote process
 *  index (OUT)    - pointer to the index used for cqe handling
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 *  This is a remote, possibly blocking operation.  Both sides must
 *  call attach (with endpoint information from the other side) for
 *  attach to succeed.  It is possible to attach multiple remote cqs
 *  into a single local cq.
 */
int ptl_cq_attach(ptl_cq_handle_t cq, ptl_cq_info_t *info);


/*
 * ptl_cq_destroy
 *
 * Destroy a command queue
 *
 * Arguments:
 *  cq (IN)        - completion queue handle
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 *  This is a local operation, but the status of a remote send / recv
 *  operation is undefined once the local process has called destroy.
 */
int ptl_cq_destroy(ptl_cq_handle_t cq);


/*
 * ptl_cq_entry_alloc
 *
 * Allocate a new command queue entry buffer
 *
 * Arguments:
 *  cq (IN)         - completion queue handle
 *  index (IN)      - remote peer index
 *  entry (OUT)     - completion queue entry pointer
 *
 * Return values:
 *  0 - success
 *  non-zero - failure
 *
 * Notes:
 */
int ptl_cq_entry_alloc(ptl_cq_handle_t cq, ptl_cqe_t** entry);


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
int ptl_cq_entry_free(ptl_cq_handle_t cq, ptl_cqe_t* entry);


/*
 * ptl_cq_entry_send
 *
 * send a command queue entry to the remote peer
 *
 * Arguments:
 *  cq (IN)       - completion queue handle
 *  entry (IN)    - completion queue entry
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
int ptl_cq_entry_send(ptl_cq_handle_t cq, int index, ptl_cqe_t *entry);


/*
 * ptl_cq_entry_recv
 *
 * send a command queue entry to the remote peer
 *
 * Return values:
 *  0 - success
 *  -1 - no messages available 
 *  else - failure
 *
 * Notes:
 *  The user must provide a buffer of the appropriate size.
 */
int ptl_cq_entry_recv(ptl_cq_handle_t cq, ptl_cqe_t *entry);

#endif

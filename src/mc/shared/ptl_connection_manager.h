#ifndef MC_CONNECTION_MANAGER_H
#define MC_CONNECTION_MANAGER_H

#include <stddef.h>

/* 
 * Opaque handle to a connection server
 */
struct ptl_cm_server_t;
typedef struct ptl_cm_server_t* ptl_cm_server_handle_t;

/* 
 * Opaque handle to a connection client
 */
struct ptl_cm_client_t;
typedef struct ptl_cm_client_t* ptl_cm_client_handle_t;

/*
 * Connection callback typedef
 *
 * Prototype for server side callback when a client establishes a
 * connection.  Remote_id is a unique identifier allocated by the
 * connection manager which may be reused immediately after a
 * disconnect callback fires.
 */
typedef int (*ptl_cm_connect_cb_t)(int remote_id, void *cb_data);

/*
 * Disconnect callback typedef
 *
 * Prototype for client and server side callback when the remote side
 * of the connection is dropped (either by the process dying or
 * calling disconnect).  Remote_id will be 0 on the client side and
 * match the remote_id specified in the connection callback on the
 * server side.
 */
typedef int (*ptl_cm_disconnect_cb_t)(int remote_id, void *cb_data);

/*
 * Receive callback typedef
 *
 * Prototype for a client and server side callback when the remote
 * side of the connection sends a message.  The invoked function must
 * be careful not to cause lock-related deadlock and may call the
 * client/server send call at most once.  The buffer will be freed by
 * the implementation when the callback returns.
 */
typedef int (*ptl_cm_recv_cb_t)(int remote_id, void *buf, size_t len, 
                                void *cb_data);


/*
 * ptl_cm_server_create
 *
 * Create the server side of the connection manager.
 *
 * Arguments:
 *  cm_h (OUT)           - connection manager handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 */
int ptl_cm_server_create(ptl_cm_server_handle_t *cm_h);


/*
 * ptl_cm_server_destroy
 *
 * Destroy the server side of the connection manager.
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 */
int ptl_cm_server_destroy(ptl_cm_server_handle_t cm_h);


/*
 * ptl_cm_server_register_connect_cb
 *
 * Register a callback to fire when a client connects to the server.
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *  cb (IN)              - callback function pointer
 *  cb_data (IN)         - handle passed to callback
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  If this function is called more than once on the same connection
 *  manager, the function specified in the last call will be invoked.
 */
int ptl_cm_server_register_connect_cb(ptl_cm_server_handle_t cm_h, 
                                      ptl_cm_connect_cb_t cb,
                                      void *cb_data);


/*
 * ptl_cm_server_register_disconnect_cb
 *
 * Register a callback to fire when a client disconnects from the server.
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *  cb (IN)              - callback function pointer
 *  cb_data (IN)         - handle passed to callback
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  If this function is called more than once on the same connection
 *  manager, the function specified in the last call will be invoked.
 */
int ptl_cm_server_register_disconnect_cb(ptl_cm_server_handle_t cm_h, 
                                         ptl_cm_disconnect_cb_t cb,
                                         void *cb_data);


/*
 * ptl_cm_server_register_recv_cb
 *
 * Register a callback to fire when a client message is received
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *  cb (IN)              - callback function pointer
 *  cb_data (IN)         - handle passed to callback
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  If this function is called more than once on the same connection
 *  manager, the function specified in the last call will be invoked.
 */
int ptl_cm_server_register_recv_cb(ptl_cm_server_handle_t cm_h, 
                                   ptl_cm_recv_cb_t cb,
                                   void *cb_data);


/*
 * ptl_cm_server_progress
 *
 * Progress the connection manager server.  Callbacks for connection,
 * disconnection, and message reception will be fired from within this
 * function.
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  This function may not be called from within a connection manager
 *  callback.
 */
int ptl_cm_server_progress(ptl_cm_server_handle_t cm_h);


/*
 * ptl_cm_server_send
 *
 * Send a message to client remote_id, starting at buf with length len
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *  remote_id (IN)       - connection manager handle
 *  buf (IN)             - connection manager handle
 *  len (IN)             - connection manager handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 */
int ptl_cm_server_send(ptl_cm_server_handle_t cm_h, int remote_id,
                       void *buf, size_t len);


/*
 * ptl_cm_client_connect
 *
 * Connect to an already established connection manager server.
 *
 * Arguments:
 *  cm_h (OUT)           - connection manager handle
 *  my_id (OUT)          - unique identifier for client
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  The connection manager server will always have id 0, which will
 *  never be an id returned from ptl_cm_client_connect.
 */
int ptl_cm_client_connect(ptl_cm_client_handle_t *cm_h, int *my_id);


/*
 * ptl_cm_client_disconnect
 *
 * Disconnect from connection manager server
 *
 * Arguments:
 *  cm_h (OUT)           - connection manager handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 */
int ptl_cm_client_disconnect(ptl_cm_client_handle_t cm_h);


/*
 * ptl_cm_client_register_disconnect_cb
 *
 * Register a callback to fire when a server disconnects from the client.
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *  cb (IN)              - callback function pointer
 *  cb_data (IN)         - handle passed to callback
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  If this function is called more than once on the same connection
 *  manager, the function specified in the last call will be invoked.
 */
int ptl_cm_client_register_disconnect_cb(ptl_cm_client_handle_t cm_h,
                                         ptl_cm_disconnect_cb_t cb,
                                         void *cb_data);


/*
 * ptl_cm_client_register_recv_cb
 *
 * Register a callback to fire when a client message is received
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *  cb (IN)              - callback function pointer
 *  cb_data (IN)         - handle passed to callback
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  If this function is called more than once on the same connection
 *  manager, the function specified in the last call will be invoked.
 */
int ptl_cm_client_register_recv_cb(ptl_cm_client_handle_t cm_h, 
                                   ptl_cm_recv_cb_t cb,
                                   void *cb_data);


/*
 * ptl_cm_client_progress
 *
 * Progress the connection manager client.  Callbacks for connection,
 * disconnection, and message reception will be fired from within this
 * function.
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 *  This function may not be called from within a connection manager
 *  callback.
 */
int ptl_cm_client_progress(ptl_cm_client_handle_t cm_h);


/*
 * ptl_cm_client_send
 *
 * Send a message to the connection manager server, starting at buf
 * with length len
 *
 * Arguments:
 *  cm_h (IN)            - connection manager handle
 *  remote_id (IN)       - connection manager handle
 *  buf (IN)             - connection manager handle
 *  len (IN)             - connection manager handle
 *
 * Return values:
 *  0                    - success
 *  -1                   - failure (errno will be set)
 *
 * Notes:
 */
int ptl_cm_client_send(ptl_cm_client_handle_t cm_h,
                       void *buf, size_t len);

#endif

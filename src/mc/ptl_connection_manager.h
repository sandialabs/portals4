#ifndef MC_CONNECTION_MANAGER_H
#define MC_CONNECTION_MANAGER_H

struct ptl_cm_server_t;
typedef struct ptl_cm_server_t* ptl_cm_server_handle_t;

struct ptl_cm_client_t;
typedef struct ptl_cm_client_t* ptl_cm_client_handle_t;

typedef int (*ptl_cm_connect_cb_t)(int remote_id);

typedef int (*ptl_cm_disconnect_cb_t)(int remote_id);


int ptl_cm_server_create(ptl_cm_server_handle_t *cm_h, int *my_id);

int ptl_cm_server_destroy(ptl_cm_server_handle_t cm_h);

int ptl_cm_server_register_connect_cb(ptl_cm_server_handle_t cm_h, 
                                      ptl_cm_connect_cb_t cb);

int ptl_cm_server_register_disconnect_cb(ptl_cm_server_handle_t cm_h, 
                                         ptl_cm_disconnect_cb_t cb);

int ptl_cm_server_progress(ptl_cm_server_handle_t cm_h);


int ptl_cm_client_connect(ptl_cm_client_handle_t *cm_h, int *my_id);

int ptl_cm_client_disconnect(ptl_cm_client_handle_t cm_h);

int ptl_cm_client_register_disconnect_cb(ptl_cm_client_handle_t cm_h,
                                         ptl_cm_disconnect_cb_t cb);

int ptl_cm_client_progress(ptl_cm_client_handle_t cm_h);

#endif

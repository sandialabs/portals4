
#ifndef _PTL3_API_TYPES_H_
#define _PTL3_API_TYPES_H_

#include <portals4.h>
typedef struct {
    ptl_nid_t nid;
    ptl_pid_t pid;
} ptl_process_id_t;

typedef struct {
    void *iov_base;
    ptl_size_t iov_len;
} ptl_md_iovec_t;


#endif

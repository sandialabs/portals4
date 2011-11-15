
#ifndef _PTL3_LIB_TYPES_H_
#define _PTL3_LIB_TYPES_H_

struct lib_nal;
typedef struct lib_ni {
    ptl_pid_t pid;
    ptl_pid_t nid;
    ptl_ni_limits_t limits; 
    unsigned int debug;
    struct lib_nal *nal;
} lib_ni_t;

typedef struct {
} ptl_hdr_t;

typedef char lib_mem_t;
typedef char api_mem_t;

#define SSIZE_T_MAX ((ptl_size_t)(~(size_t)0>>1))

#endif

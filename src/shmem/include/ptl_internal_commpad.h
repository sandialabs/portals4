#ifndef PTL_INTERNAL_COMMPAD_H
#define PTL_INTERNAL_COMMPAD_H

#include <stddef.h>		       /* for size_t */

extern volatile char *comm_pad;
extern size_t num_siblings;
extern size_t proc_number;
extern size_t per_proc_comm_buf_size;
extern size_t firstpagesize;

typedef struct
{
    unsigned char ni;
    unsigned char type;
    union {
	struct {
	} put;
	struct {
	} get;
	struct {
	} atomic;
	struct {
	} fetchatomic;
	struct {
	} swap;
    } info;
    volatile void * volatile next;
} ptl_internal_header_t;

extern volatile ptl_internal_header_t *ops;


#endif

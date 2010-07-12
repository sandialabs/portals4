#ifndef PTL_INTERNAL_COMMPAD_H
#define PTL_INTERNAL_COMMPAD_H

#include <stddef.h>		       /* for size_t */

extern volatile char *comm_pad;
extern size_t num_siblings;
extern size_t proc_number;
extern size_t per_proc_comm_buf_size;
extern size_t firstpagesize;

#endif

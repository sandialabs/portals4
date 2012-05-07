#ifndef PORTALS4_RUNTIME_H
#define PORTALS4_RUNTIME_H

struct runtime_proc_t {
    ptl_nid_t nid;
    ptl_pid_t pid;
};

void runtime_init(void);
void runtime_finalize(void);

int runtime_get_rank(void);
int runtime_get_size(void);
int runtime_get_nidpid_map(struct runtime_proc_t**);
void runtime_barrier(void);

#endif

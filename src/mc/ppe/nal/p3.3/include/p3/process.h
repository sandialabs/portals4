#ifndef _PTL3_P3_PROCESS_H_
#define _PTL3_P3_PROCESS_H_

typedef struct p3_process {

    int init;   /* interface count, or -1 if PtlInit not called. */

    ptl_jid_t jid;

    void *ni[PTL_MAX_INTERFACES];   /* interfaces for this process */

} p3_process_t;

#endif

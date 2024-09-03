// put some unexpected puts to rank 1
// barrier
// have rank 1 append an ME that matches zero or more of them
// barrier
// clean up
#include <portals4.h>
#include <support.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include "testing.h"
#define ENTRY_T  ptl_me_t
#define HANDLE_T ptl_handle_me_t
#define NI_TYPE  PTL_NI_MATCHING
// for catching unexpected puts
#define AOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE)
#define SOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE)
#define ME_OPTIONS (PTL_ME_OP_PUT | PTL_ME_MANAGE_LOCAL | PTL_ME_LOCAL_INC_UH_RLENGTH)
#define APPEND   PtlMEAppend
#define UNLINK   PtlMEUnlink
#define SEARCH   PtlMESearch // buffer size (in uint64_t) of ME appended by rank 1

struct thread_data {
    int name;
    ptl_handle_eq_t  eq_h;
    ptl_event_t      event;
};

void* thread_test(void* arg) {
    printf("I'm a worker thread testing!\n");
    return (void*) 42;
}

    
void* thread_poll(void* arg) {
    void * ret;
    int err;
    unsigned int which;
    unsigned polltime_ms = 2000;
    struct thread_data *data = (struct thread_data*) arg; 
    printf("worker[%d]: calling PtlEQPoll\n", data->name);
    
    err = PtlEQPoll(&data->eq_h, 1, polltime_ms, &data->event, &which);
    if (err == PTL_ABORTED) {
        printf("worker[%d]: PTL_ABORTED returned\n", data->name);
    } else {
        printf("worker[%d]: PTL_ABORTED NOT returned\n", data->name);
    }
    ret = (void*) err;
    return ret;
}

void* thread_abort(void* arg) {
    struct thread_data *data = (struct thread_data*) arg; 
    sleep(1);
    printf("worker[%d]: calling PtlAbort\n", data->name);
    PtlAbort();
}


int main(int   argc, char *argv[])
{
    ptl_handle_ni_t  ni_h;
    ptl_pt_index_t   pt_index;
    uint64_t         value;
    ENTRY_T          value_e;
    HANDLE_T         value_e_handle;
    ptl_md_t         write_md;
    ptl_handle_md_t  write_md_handle;
    int              num_procs, error_found, ret;
    ptl_ct_event_t   ctc;
    int              rank;
    ptl_process_t   *procs;
    struct thread_data tdata0;
    struct thread_data tdata1;
    struct thread_data tdata2;
    ptl_handle_eq_t  eq_h;
    ptl_event_t      event;
    ENTRY_T          append_me; // the ME to be appended
    HANDLE_T         append_me_handle; // handle for the ME to be appended
    
    const uint64_t num_puts    = 5;
    const uint64_t ME_BUF_SIZE = 6;
    const uint64_t MIN_FREE    = 30;
    unsigned int loffset       =  (ME_BUF_SIZE+1)*sizeof(uint64_t) - MIN_FREE;
    loffset = loffset - (loffset % sizeof(uint64_t));
    printf("loffset = %lu\n", loffset);
    
    uint64_t me_buffer[ME_BUF_SIZE]; // buffer used by the ME being appended

    


    for (int i = 0; i < ME_BUF_SIZE; ++i)
        me_buffer[i] = i;

    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(libtest_init());
    rank      = libtest_get_rank();
    num_procs = libtest_get_size();
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));

    procs     = libtest_get_mapping(ni_h);

    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));

    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                               &pt_index));
    
    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &tdata1.eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, tdata1.eq_h, PTL_PT_ANY,
                               &pt_index));

    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &tdata0.eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, tdata0.eq_h, PTL_PT_ANY,
                               &pt_index));
    
    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &tdata2.eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, tdata2.eq_h, PTL_PT_ANY,
                               &pt_index));
    //assert(pt_index == 0);

    
    if (rank == 0) {
        int err;
        unsigned int which;
        unsigned polltime_ms = 5000;
        int ret;

        
        // TODO dkruse this is where we poll/wait for stuff then abort
        pthread_attr_t tattr0;
        pthread_t worker0;
        void *status0;

        pthread_attr_t tattr1;
        pthread_t worker1;
        void *status1;

        pthread_attr_t tattr2;
        pthread_t worker2;
        void *status2;

        tdata0.name = 0;
        tdata1.name = 1;
        tdata2.name = 2;
        
        /* parent thread aborts, 2 worker threads poll */
        printf("parent thread aborts, 2 worker threads poll\n");
        ret = pthread_create(&worker0, NULL, thread_poll, &tdata0);
        ret = pthread_create(&worker1, NULL, thread_poll, &tdata1);
        sleep(1);
        PtlAbort();

        ret = pthread_join(worker0, &status0);
        if (ret)
            printf("worker0 pthread_join error\n");
            
        ret = pthread_join(worker1, &status1);
        if (ret)
            printf("worker1 pthread_join error\n");

        printf("PTL_ABORTED is %d\n", PTL_ABORTED);
        printf("worker0 returned %d\n", (int) status0);
        printf("worker1 returned %d\n", (int) status1);
        assert((int)status0 == PTL_ABORTED);
        assert((int)status1 == PTL_ABORTED);

        ret = PtlEQFree(tdata0.eq_h);
        printf("worker0 ret = %d\n", ret);
        ret = PtlEQFree(tdata1.eq_h);
        printf("worker1 ret = %d\n", ret);
        printf("PTL_OK == %d\n", PTL_OK);
  
        ret = PtlEQAlloc(ni_h, 8192, &eq_h);
        printf("eqAlloc: worker0 ret = %d\n", ret);
        ret = PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                         &pt_index);
        printf("ptAlloc: worker0 ret = %d\n", ret);
    
        ret = PtlEQAlloc(ni_h, 8192, &tdata1.eq_h);
        printf("eqAlloc: worker1 ret = %d\n", ret);
        ret = PtlPTAlloc(ni_h, 0, tdata1.eq_h, PTL_PT_ANY,
                                   &pt_index);
        printf("ptAlloc: worker1 ret = %d\n", ret);

        
        printf("\n////////////////////////////////////////////////////////////////////////////////\n");
        printf("////////////////////////////////////////////////////////////////////////////////\n\n");


        /* sanity check */
        printf("sanity check:\n");
        ret = pthread_create(&worker2, NULL, thread_test, &tdata2);
        ret = pthread_join(worker2, &status2);
        printf("\n");

        
        /* parent thread aborts, 2 worker threads poll again */
        printf("parent thread aborts, 2 worker threads poll again\n");
        ret = pthread_create(&worker0, NULL, thread_poll, &tdata0);
        if (ret)
            printf("worker0 pthread_create error\n");
        
        ret = pthread_create(&worker1, NULL, thread_poll, &tdata1);
        if (ret)
            printf("worker1 pthread_create error\n");
        //err = PtlEQPoll(&eq_h, 1, polltime_ms, &event, &which);
        //
        sleep(1);
        PtlAbort();
        ret = pthread_join(worker0, &status0);
        if (ret)
            printf("worker0 pthread_join error\n");
            
        ret = pthread_join(worker1, &status1);
        if (ret)
            printf("worker1 pthread_join error\n");

        printf("PTL_ABORTED is %d\n", PTL_ABORTED);
        printf("worker0 returned %d\n", (int) status0);
        printf("worker1 returned %d\n", (int) status1);
        assert((int)status0 == PTL_ABORTED);
        
        ret = PtlEQFree(tdata0.eq_h);
        printf("worker0 ret = %d\n", ret);
        ret = PtlEQFree(tdata1.eq_h);
        printf("worker1 ret = %d\n", ret);
        printf("PTL_OK == %d\n", PTL_OK);
  
        ret = PtlEQAlloc(ni_h, 8192, &eq_h);
        printf("eqAlloc: worker0 ret = %d\n", ret);
        ret = PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                         &pt_index);
        printf("ptAlloc: worker0 ret = %d\n", ret);
    
        ret = PtlEQAlloc(ni_h, 8192, &tdata1.eq_h);
        printf("eqAlloc: worker1 ret = %d\n", ret);
        ret = PtlPTAlloc(ni_h, 0, tdata1.eq_h, PTL_PT_ANY,
                                   &pt_index);
        printf("ptAlloc: worker1 ret = %d\n", ret);


        
        printf("\n////////////////////////////////////////////////////////////////////////////////\n");
        printf("////////////////////////////////////////////////////////////////////////////////\n\n");
        
        /* sanity check */
        printf("sanity check:\n");
        ret = pthread_create(&worker2, NULL, thread_test, &tdata2);
        ret = pthread_join(worker2, &status2);
        printf("\n");

        
        /* parent thread and worker0 poll, worker2 aborts*/
        printf("parent thread aborts, 2 worker threads poll again\n");
        ret = pthread_create(&worker0, NULL, thread_poll, &tdata0);
        if (ret)
            printf("worker0 pthread_create error\n");
        
        ret = pthread_create(&worker1, NULL, thread_abort, &tdata1);
        if (ret)
            printf("worker1 pthread_create error\n");
        
        err = PtlEQPoll(&eq_h, 1, polltime_ms, &event, &which);
        

        
        ret = pthread_join(worker0, &status0);
        if (ret)
            printf("worker0 pthread_join error\n");
            
        ret = pthread_join(worker1, &status1);
        if (ret)
            printf("worker1 pthread_join error\n");

        printf("PTL_ABORTED is %d\n", PTL_ABORTED);
        printf("worker0 returned %d\n", (int) status0);
        printf("parent returned %d\n", err);
        assert((int)status0 == PTL_ABORTED);
        
        ret = PtlEQFree(tdata0.eq_h);
        printf("worker0 ret = %d\n", ret);
        ret = PtlEQFree(tdata1.eq_h);
        printf("worker1 ret = %d\n", ret);
        printf("PTL_OK == %d\n", PTL_OK);
  
        ret = PtlEQAlloc(ni_h, 8192, &eq_h);
        printf("eqAlloc: worker0 ret = %d\n", ret);
        ret = PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                         &pt_index);
        printf("ptAlloc: worker0 ret = %d\n", ret);
    
        ret = PtlEQAlloc(ni_h, 8192, &tdata1.eq_h);
        printf("eqAlloc: worker1 ret = %d\n", ret);
        ret = PtlPTAlloc(ni_h, 0, tdata1.eq_h, PTL_PT_ANY,
                                   &pt_index);
        printf("ptAlloc: worker1 ret = %d\n", ret);
    }
    


    libtest_barrier();

    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();
    return 0;
}

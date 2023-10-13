#include <stdio.h>
#include <stdlib.h>
#include <portals4.h>
#include <inttypes.h>
#include <assert.h>
#include <support.h>
#include "testing.h"

int main(int argc, char *argv[])
{
	int			err;
	ptl_interface_t		iface;
	unsigned int		ni_opt;
	ptl_process_t		id;
	ptl_ni_limits_t		desired;
	ptl_ni_limits_t		actual;
	ptl_handle_ni_t		ni_handle;
	unsigned int		pt_opt;
	ptl_handle_eq_t		eq_handle;
	ptl_pt_index_t		pt_index_req;
	ptl_pt_index_t		pt_index;
	ptl_me_t		me;
	ptl_list_t		ptl_list;
	void			*user_ptr;
	ptl_handle_me_t 	me_handles[20];
	ptl_md_t		md;
	ptl_handle_md_t		md_handle;
	ptl_size_t		local_offset;
	ptl_size_t		length;
	ptl_ack_req_t		ack_req;
	ptl_process_t		target_id;
	ptl_match_bits_t	match_bits;
	ptl_size_t		remote_offset;
	ptl_hdr_data_t		hdr_data;
	int			i, j;
        ptl_size_t              count;
	ptl_event_t		event;

	/*
	 * init portals library
	 */
	err = PtlInit();
	if (err) {
		printf("PtlInit failed, err = %d\n", err);
		return 1;
	}

	/*
	 * create an NI
	 */
	iface				= PTL_IFACE_DEFAULT;
	ni_opt				= PTL_NI_MATCHING | PTL_NI_PHYSICAL;
	id.phys.nid			= PTL_NID_ANY;
	id.phys.pid			= PTL_PID_ANY;
	desired.max_entries		= 64;
	desired.max_mds			= 64;
	desired.max_cts			= 64;
	desired.max_eqs			= 64;
	desired.max_pt_index		= 64;
	desired.max_iovecs		= 64;
	desired.max_list_size		= 64;
	desired.max_msg_size		= 32768;
	desired.max_atomic_size		= 64;

	err = PtlNIInit(iface, ni_opt, PTL_PID_ANY, &desired, &actual,
		      &ni_handle);
	if (err) {
		printf("PtlNIInit failed, err = %d\n", err);
		return 1;
	}

	/*
	 * get process ID
	 */
	err = PtlGetId(ni_handle, &id);
	if (err) {
		printf("PtlGetId failed, err = %d\n", err);
		return 1;
	}

	/*
	 * create an EQ
	 */
	count		= 1000;

	err = PtlEQAlloc(ni_handle, count, &eq_handle);
	if (err) {
		printf("PtlEQAlloc failed, err = %d\n", err);
		return 1;
	}

	/*
	 * create portals table entry
	 */
	pt_opt		= 0;
	pt_index_req	= PTL_PT_ANY;

	err = PtlPTAlloc(ni_handle, pt_opt, PTL_EQ_NONE, pt_index_req, &pt_index);
	if (err) {
		printf("PtlPTAlloc failed, err = %d\n", err);
		return 1;
	}

	/*
	 * create ME
	 */
	me.length	= 1024;
	me.start	= malloc(me.length);
	me.ct_handle	= PTL_CT_NONE;
	me.uid	= PTL_UID_ANY;
	me.options	= PTL_ME_OP_PUT | PTL_ME_USE_ONCE;
	me.min_free	= 0;
	me.match_id	= id;
	me.match_bits	= 0;
	me.ignore_bits	= 0;
	ptl_list	= PTL_PRIORITY_LIST;
	user_ptr	= NULL;
	if (!me.start) {
		printf("unable to allocate %" PRIu64 " bytes for me\n", me.length);
		return 1;
	}

/*
                 * create MD
                 */
                md.length		= 1024;
                md.start		= malloc(md.length);
                md.options		= 0;
                md.eq_handle		= eq_handle;
                md.ct_handle		= PTL_CT_NONE;
                if (!md.start) {
                        printf("unable to allocate %" PRIu64 " bytes for md\n", md.length);
                        return 1;
                }

                err = PtlMDBind(ni_handle, &md, &md_handle);
                if (err) {
                        printf("PtlMDBind failed, err = %d\n", err);
                        return 1;
                }



        for (j = 0; j < 1000; j++){
                libtest_barrier();
                for (i = 0; i < 20; i++) {
                        err = PtlMEAppend(ni_handle, pt_index, &me, ptl_list, user_ptr, &me_handles[i]);
                        if (err) {
                                printf("PtlMEAppend failed, err = %d\n", err);
                                return 1;
                        }
                }

                local_offset            = 0;
                length                  = 4;
                ack_req                 = PTL_ACK_REQ;
                target_id               = id;
                match_bits              = 0;
                remote_offset           = 0;
                hdr_data                = 0;
                for (i = 0; i < 10; i++) {
                        err = PtlPut(md_handle, local_offset, length, ack_req, target_id,
                                     pt_index, match_bits, remote_offset, user_ptr, hdr_data);
                        if (err) {
                                printf("PtlPut failed, err = %d\n", err);
                                return 1;
                        }
                }

                for (i = 0; i < 10; i++) {
                        err = PtlEQWait(eq_handle, &event);
                        if (err) {
                                printf("PtlEQWait failed, err = %d\n", err);
                                return 1;
                        }
                }

                int unlinks = 0;
                for (i = 0;i < 20; i++) {
                        err = PtlMEUnlink(me_handles[i]);
                        if (err == PTL_OK) {
                                unlinks++;
                        }
                        if (unlinks == 10) {
                                break;
                        }
                }
        //        printf("asserts= %d\n",unlinks);
                assert(unlinks == 10);

                for (i = 0; i < 10; i++) {
                        err = PtlEQWait(eq_handle, &event);
                        if (err) {
                                printf("PtlEQWait failed, err = %d\n", err);
                                return 1;
                        }
                }

        }

	/*
	 * destroy MD
	 */
	do {
		err = PtlMDRelease(md_handle);
		if (err && err != PTL_IN_USE) {
			printf("PtlMDRelease failed, err = %d\n", err);
			return 1;
		}
	} while(err == PTL_IN_USE);

        libtest_barrier();
 
	/*
	 * destroy portals table entry
	 */
        err = PtlPTFree(ni_handle, pt_index);
        if (err && err != PTL_PT_IN_USE) {
   	    printf("PtlPTFree failed, err = %d\n", err);
	    return 1;
        }

	/*
	 * destroy an EQ
	 */
	err = PtlEQFree(eq_handle);
	if (err) {
		printf("PtlEQFree failed, err = %d\n", err);
		return 1;
	}

	/*
	 * destroy an NI
	 */
	err = PtlNIFini(ni_handle);
	if (err) {
		printf("PtlNIFini failed, err = %d\n", err);
		return 1;
	}

	/*
 	 * cleanup portals library
	 */
	PtlFini();

	return 0;
}

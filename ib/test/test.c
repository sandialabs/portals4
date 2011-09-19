#include <stdio.h>
#include <stdlib.h>
#include <portals4.h>
#include <sys/time.h>
#include <inttypes.h>

static int debug = 0;
static int put = 0;
int ptl_log_level;

static void dump_limits(ptl_ni_limits_t *limits)
{
	printf("max_entries		= %d\n", limits->max_entries);
	printf("max_mds			= %d\n", limits->max_mds);
	printf("max_cts			= %d\n", limits->max_cts);
	printf("max_eqs			= %d\n", limits->max_eqs);
	printf("max_pt_index		= %d\n", limits->max_pt_index);
	printf("max_iovecs		= %d\n", limits->max_iovecs);
	printf("max_list_size		= %d\n", limits->max_list_size);
	printf("max_msg_size		= %" PRIu64 "\n", limits->max_msg_size);
	printf("max_atomic_size		= %" PRIu64 "\n", limits->max_atomic_size);
}

static void dump_event(ptl_event_t *event)
{
        printf("type			= %d\n", event->type);
        printf("initiator.phys.nid	= 0x%x\n", event->initiator.phys.nid);
        printf("initiator.phys.pid	= 0x%x\n", event->initiator.phys.pid);
        printf("pt_index		= %d\n", event->pt_index);
        printf("uid			= %d\n", event->uid);
        printf("jid			= %d\n", event->jid);
        printf("match_bits		= 0x%" PRIx64 "\n", event->match_bits);
        printf("rlength			= %" PRIu64 "\n", event->rlength);
        printf("mlength			= %" PRIu64 "\n", event->mlength);
        printf("remote_offset		= %" PRIu64 "\n", event->remote_offset);
        printf("start			= %p\n", event->start);
        printf("user_ptr		= %p\n", event->user_ptr);
        printf("hdr_data		= %" PRIu64 "\n", event->hdr_data);
        printf("ni_fail_type		= %d\n", event->ni_fail_type);
        printf("atomic_operation	= %d\n", event->atomic_operation);
        printf("atomic_type		= %d\n", event->atomic_type);
}

static struct timeval start_time;
static struct timeval stop_time;
int test_time;

int main(int argc, char *argv[])
{
	int			err;
	double			elapsed_time;
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
	ptl_handle_me_t 	me_handle;
	ptl_md_t		md;
	ptl_handle_md_t		md_handle;
	ptl_size_t		local_offset;
	ptl_size_t		length;
	ptl_ack_req_t		ack_req;
	ptl_process_t		target_id;
	ptl_match_bits_t	match_bits;
	ptl_size_t		remote_offset;
	ptl_hdr_data_t		hdr_data;
	int			i;
        ptl_size_t              count;
	ptl_event_t		event;

	ptl_log_level = 4;

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

	if (debug > 1)
		dump_limits(&actual);

	/*
	 * get process ID
	 */
	err = PtlGetId(ni_handle, &id);
	if (err) {
		printf("PtlGetId failed, err = %d\n", err);
		return 1;
	}

	if (debug)
		printf("NID = %x, PID = %x\n", id.phys.nid, id.phys.pid);

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

	if (debug > 1)
		printf("PtlPTAlloc returned pt_index = %d\n", pt_index);

	/*
	 * create ME
	 */
	me.length	= 1024;
	me.start	= malloc(me.length);
	me.ct_handle	= PTL_CT_NONE;
	me.ac_id.jid	= 0;
	me.ac_id.uid	= PTL_UID_ANY;
	me.options	= PTL_ME_OP_PUT | PTL_ME_OP_GET;
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

	err = PtlMEAppend(ni_handle, pt_index, &me, ptl_list, user_ptr, &me_handle);
	if (err) {
		printf("PtlMDBind failed, err = %d\n", err);
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

	/*
	 * do a put/get to warm up
	 */
	local_offset		= 0;
	length			= 4;
	ack_req			= PTL_ACK_REQ;
	target_id		= id;
	match_bits		= 0;
	remote_offset		= 0;
	hdr_data		= 0;

	if (put) {
		err = PtlPut(md_handle, local_offset, length, ack_req, target_id,
			     pt_index, match_bits, remote_offset, user_ptr, hdr_data);
		if (err) {
			printf("PtlPut failed, err = %d\n", err);
			return 1;
		}

		for (i = 0; i < 2; i++) {
			err = PtlEQWait(eq_handle, &event);
			if (err) {
				printf("PtlEQWait failed, err = %d\n", err);
				return 1;
			}
		}
	} else {
		err = PtlGet(md_handle, local_offset, length, target_id,
			     pt_index, match_bits, remote_offset, user_ptr);
		if (err) {
			printf("PtlGet failed, err = %d\n", err);
			return 1;
		}

		err = PtlEQWait(eq_handle, &event);
		if (err) {
			printf("PtlEQWait failed, err = %d\n", err);
			return 1;
		}
	}

	gettimeofday(&start_time, NULL);
	gettimeofday(&stop_time, NULL);

	printf("=============START=================\n");

	gettimeofday(&start_time, NULL);

	if (put) {
		for (i = 0; i < 10; i++) {
			err = PtlPut(md_handle, local_offset, length, ack_req, target_id,
				     pt_index, match_bits, remote_offset, user_ptr, hdr_data);
			if (err) {
				printf("PtlPut failed, err = %d\n", err);
				return 1;
			}
		}

		for (i = 0; i < 20; i++) {
			err = PtlEQWait(eq_handle, &event);
			if (err) {
				printf("PtlEQWait failed, err = %d\n", err);
				return 1;
			}
		}
	} else {
		for (i = 0; i < 400; i++) {
			err = PtlGet(md_handle, local_offset, length, target_id,
				     pt_index, match_bits, remote_offset, user_ptr);
			if (err) {
				printf("PtlGet failed, err = %d\n", err);
				return 1;
			}
		}

		for (i = 0; i < 400; i++) {
			err = PtlEQWait(eq_handle, &event);
			if (err) {
				printf("PtlEQWait failed, err = %d\n", err);
				return 1;
			}
		}
	}

	gettimeofday(&stop_time, NULL);

	printf("=============STOP=================\n");

	if (1)
		dump_event(&event);

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

	/*
	 * destroy ME
	 */
	do {
		err = PtlMEUnlink(me_handle);
		if (err && err != PTL_IN_USE) {
			printf("PtlMEUnlink failed, err = %d\n", err);
			return 1;
		}
	} while(err == PTL_IN_USE);

	/*
	 * destroy portals table entry
	 */
	err = PtlPTFree(ni_handle, pt_index);
	if (err) {
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

	elapsed_time = (stop_time.tv_sec - start_time.tv_sec)
			+ 1e-6*(stop_time.tv_usec - start_time.tv_usec);

	printf("elapsed time = %lf\n", elapsed_time);

	return 0;
}

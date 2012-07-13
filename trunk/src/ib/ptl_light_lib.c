/**
 * @file ptl_ppe_client.c
 *
 * @brief Client side for PPE support.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ptl_loc.h"

#include <sys/un.h>

/*
 * per process global state
 * acquire proc_gbl_mutex before making changes
 * that require atomicity
 */
static pthread_mutex_t per_proc_gbl_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct {
	/* Communication with PPE. */
	struct ppe_comm_pad *ppe_comm_pad;

	/* Lifeline to the PPE. */
	int s;

	/* Cookie given by the PPE to that client and used for almost
	 * any communication. */
	void *cookie;

	/* Count PtlInit/PtlFini */
	int	ref_cnt;
	int finalized;

	/* Virtual address of the slab containing the ppebufs on the
	 * PPE. */
	void *ppebufs_ppeaddr;

	/* Virtual address of the slab containing the ppebufs in this
	 * process. */
	void *ppebufs_addr;

	/* Offset of the ppebuf slab in the comm_pad */
	off_t ppebufs_offset;

	/* Points to own queue in comm pad. */
	queue_t *queue;

	/* XPMEM segid for that whole process. */
	xpmem_segid_t segid;
} ppe;

void gbl_release(ref_t *ref)
{
}

int gbl_init(gbl_t *gbl)
{
	return PTL_OK;
}

/**
 * Allocate a ppebuf from the shared memory pool.
 *
 * @param buf_p pointer to return value
 *
 * @return status
 */
static inline int ppebuf_alloc(ppebuf_t **buf_p)
{
	obj_t *obj;

#ifndef NO_ARG_VALIDATION
	if (!ppe.ppe_comm_pad)
		return PTL_NO_INIT;
#endif

	while ((obj = ll_dequeue_obj_alien(&ppe.ppe_comm_pad->ppebuf_pool.free_list,
										 ppe.ppebufs_addr, ppe.ppebufs_ppeaddr)) == NULL) {
		SPINLOCK_BODY();
	}

	*buf_p = container_of(obj, ppebuf_t, obj);
	return PTL_OK;
}

/**
 * Drop a reference to a ppebuf
 *
 * If the last reference has been dropped the buf
 * will be freed.
 *
 * @param buf on which to drop a reference
 *
 * @return status
 */
static inline void ppebuf_release(ppebuf_t *buf)
{
	obj_t *obj = (obj_t *)buf;

	__sync_synchronize();

	ll_enqueue_obj_alien(&ppe.ppe_comm_pad->ppebuf_pool.free_list, obj,
						 ppe.ppebufs_addr, ppe.ppebufs_ppeaddr);
}

/* Attach to an XPMEM segment. */
static void *map_segment(struct xpmem_map *mapping)
{
	off_t offset;

	mapping->addr.apid = xpmem_get(mapping->segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL);
	if (mapping->addr.apid == -1) {
		WARN();
		return NULL;
	}

	/* Hack. When addr.offset is not page aligned, xpmem_attach()
	 * always fail. So fix the ptr afterwards. */
	offset = ((uintptr_t)mapping->source_addr) & (pagesize-1);
	mapping->addr.offset = mapping->offset - offset;
	mapping->ptr_attach = xpmem_attach(mapping->addr, mapping->size+offset, NULL);
	if (mapping->ptr_attach == (void *)-1) {
		WARN();
		xpmem_release(mapping->addr.apid);
		return NULL;
	}

	return mapping->ptr_attach + offset;
}

/* Detach from an XPMEM segment. */
static void unmap_segment(struct xpmem_map *mapping)
{
	if (xpmem_detach(mapping->ptr_attach) != 0) {
		WARN();
		return;
	}

	xpmem_release(mapping->addr.apid);
}

static inline void fill_mapping_info(const void *addr_in, size_t length,
									 struct xpmem_map *mapping)
{
	mapping->offset = (uintptr_t)addr_in; /* segid is for whole space */
	mapping->source_addr = addr_in;
	mapping->size = length;
	mapping->segid = ppe.segid;
}

/**
 * @brief Cleanup shared memory resources.
 *
 * @param[in] ni
 */
static void release_ppe_resources(void)
{
	if (ppe.segid != -1)
		xpmem_remove(ppe.segid);

	if (ppe.s != -1)
		close(ppe.s);

	if (ppe.ppebufs_addr)
		unmap_segment(ppe.ppebufs_addr);
}

/* Establish the link with the PPE. */
static int connect_to_ppe(void)
{
	struct sockaddr_un ppe_sock_addr;
	union msg_ppe_client msg;
	size_t len;

	ppe.s = -1;

	/* XPMEM the whole memory of that process. */
	ppe.segid = xpmem_make(0, 0xffffffffffffffffUL,
						   XPMEM_PERMIT_MODE, (void *)0600);
	if (ppe.segid == -1)
		goto exit_fail;

	/* Connect to the PPE. */
	if ((ppe.s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		ptl_warn("Couldn't create a unix socket.\n");
		goto exit_fail;
    }

	ppe_sock_addr.sun_family = AF_UNIX;
    strcpy(ppe_sock_addr.sun_path, PPE_SOCKET_NAME);
    if (connect(ppe.s, (struct sockaddr *)&ppe_sock_addr, sizeof(ppe_sock_addr)) == -1) {
		ptl_warn("Connection to PPE failed.\n");
		goto exit_fail;
    }

	/* Connected. Send the request. */
	msg.req.pid = getpid();
	msg.req.segid = ppe.segid;

	if (send(ppe.s, &msg, sizeof(msg), 0) == -1) {
		ptl_warn("Failed to say hello to PPE.\n");
		goto exit_fail;
    }

	/* Wait for the reply. */
	len = recv(ppe.s, &msg, sizeof(msg), 0);
	if (len != sizeof(msg)) {
		WARN();
		goto exit_fail;
	}

	/* Process the reply. */
	if (msg.rep.ret != PTL_OK) {
		WARN();
		goto exit_fail;
	}

	ppe.ppe_comm_pad = map_segment(&msg.rep.ppebufs_mapping);
	if (ppe.ppe_comm_pad == NULL) {
		WARN();
		goto exit_fail;
	}

	ppe.cookie = msg.rep.cookie;
	ppe.ppebufs_ppeaddr = msg.rep.ppebufs_ppeaddr;
	ppe.ppebufs_addr = ppe.ppe_comm_pad->ppebuf_slab;

	ppe.ppebufs_offset = ppe.ppebufs_addr - ppe.ppebufs_ppeaddr;
	ppe.queue = &ppe.ppe_comm_pad->q[msg.rep.queue_index].queue;

	/* This client can now communicate through regular messages with the PPE. */
	return PTL_OK;

 exit_fail:
	release_ppe_resources();

	return PTL_FAIL;
}

/* Transfer a message to the PPE and busy wait for the reply. */
static void transfer_msg(ppebuf_t *buf)
{
	/* Enqueue on the PPE queue. Since we know the virtual address on
	 * the PPE, that is what we give, and why we pass NULL as 1st
	 * parameter. */
	buf->obj.next = NULL;
	buf->completed = 0;
	buf->cookie = ppe.cookie;

	enqueue((void *)(uintptr_t)ppe.ppebufs_offset, ppe.queue, (obj_t *)buf);

	/* Wait for the reply from the PPE. */
	while(buf->completed == 0)
		SPINLOCK_BODY();
}

int PtlInit(void)
{
	int ret;
	ppebuf_t *buf;

	ret = pthread_mutex_lock(&per_proc_gbl_mutex);
	if (ret) {
		ptl_warn("unable to acquire proc_gbl mutex\n");
		ret = PTL_FAIL;
		goto err0;
	}

	if (ppe.finalized) {
		ptl_warn("PtlInit after PtlFini\n");
		ret = PTL_FAIL;
		goto err1;
	}

	/* if first call to PtlInit do real initialization */
	if (ppe.ref_cnt == 0) {
		if (misc_init_once() != PTL_OK) {
			goto err1;
		}

		ret = connect_to_ppe();
		if (ret != PTL_OK) {
			goto err1;
		}
	}

	/* Call PPE now. */
	if ((ret = ppebuf_alloc(&buf))) {
		WARN();
		goto err1;
	}

	buf->op = OP_PtlInit;

	transfer_msg(buf);

	ret = buf->msg.ret;

	ppebuf_release(buf);

	if (ret)
		goto err0;

	ppe.ref_cnt++;

	pthread_mutex_unlock(&per_proc_gbl_mutex);

	return PTL_OK;

 err1:
	pthread_mutex_unlock(&per_proc_gbl_mutex);
 err0:
	return ret;
}

void PtlFini(void)
{
	int ret;

	ret = pthread_mutex_lock(&per_proc_gbl_mutex);
	if (ret) {
		ptl_warn("unable to acquire proc_gbl mutex\n");
		abort();
		goto err0;
	}

	/* this would be a bug */
	if (ppe.ref_cnt == 0) {
		ptl_warn("ref_cnt already 0 ?!!\n");
		goto err1;
	}

	ppe.ref_cnt--;

	if (ppe.ref_cnt == 0) {
		ppebuf_t *buf;
	
		ppe.finalized = 1;

		/* Call PPE now. */
		if ((ret = ppebuf_alloc(&buf))) {
			WARN();
			goto err1;
		}
		
		buf->op = OP_PtlFini;

		transfer_msg(buf);

		ret = buf->msg.ret;

		ppebuf_release(buf);

		release_ppe_resources();
	}

	pthread_mutex_unlock(&per_proc_gbl_mutex);

	return;

err1:
	pthread_mutex_unlock(&per_proc_gbl_mutex);
err0:
	return;
}

/* Passthrough operations. */

int PtlNIInit(ptl_interface_t        iface,
              unsigned int           options,
              ptl_pid_t              pid,
              const ptl_ni_limits_t *desired,
              ptl_ni_limits_t       *actual,
              ptl_handle_ni_t       *ni_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlNIInit;

	buf->msg.PtlNIInit.iface = iface;
	buf->msg.PtlNIInit.options = options;
	buf->msg.PtlNIInit.pid = pid;
	if (desired) {
		buf->msg.PtlNIInit.with_desired = 1;
		buf->msg.PtlNIInit.desired = *desired;
	} else {
		buf->msg.PtlNIInit.with_desired = 0;
	}
	transfer_msg(buf);

	err = buf->msg.ret;

	if (actual)
		*actual = buf->msg.PtlNIInit.actual;

	*ni_handle = buf->msg.PtlNIInit.ni_handle;

	ppebuf_release(buf);

	return err;
}

int PtlNIFini(ptl_handle_ni_t ni_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	for(;;) {
		buf->op = OP_PtlNIFini;

		buf->msg.PtlNIFini.ni_handle = ni_handle;

		transfer_msg(buf);

		err = buf->msg.ret;

		if (err == PTL_IN_USE)
			/* The NI is not fully disconnected yet. Retry. This is an
			 * internal error, not seen by the application. */
			usleep(100000);
		else
			break;
	}

	ppebuf_release(buf);

	return err;
}

int PtlNIStatus(ptl_handle_ni_t ni_handle,
                ptl_sr_index_t  status_register,
                ptl_sr_value_t *status)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}


	buf->op = OP_PtlNIStatus;

	buf->msg.PtlNIStatus.ni_handle = ni_handle;
	buf->msg.PtlNIStatus.status_register = status_register;

	transfer_msg(buf);

	err = buf->msg.ret;

	*status = buf->msg.PtlNIStatus.status;

	ppebuf_release(buf);

	return err;
}

int PtlNIHandle(ptl_handle_any_t handle,
                ptl_handle_ni_t *ni_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlNIHandle;

	buf->msg.PtlNIHandle.handle = handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	*ni_handle = buf->msg.PtlNIHandle.ni_handle;

	ppebuf_release(buf);

	return err;
}

int PtlSetMap(ptl_handle_ni_t      ni_handle,
              ptl_size_t           map_size,
              const ptl_process_t *mapping)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlSetMap;

	buf->msg.PtlSetMap.ni_handle = ni_handle;
	buf->msg.PtlSetMap.map_size = map_size;
	buf->msg.PtlSetMap.mapping = mapping;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlGetMap(ptl_handle_ni_t ni_handle,
              ptl_size_t      map_size,
              ptl_process_t  *mapping,
              ptl_size_t     *actual_map_size)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlGetMap;

	buf->msg.PtlGetMap.ni_handle = ni_handle;
	buf->msg.PtlGetMap.map_size = map_size;
	buf->msg.PtlGetMap.mapping = mapping;

	transfer_msg(buf);

	*actual_map_size = buf->msg.PtlGetMap.actual_map_size;

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlPTAlloc(ptl_handle_ni_t ni_handle,
               unsigned int    options,
               ptl_handle_eq_t eq_handle,
               ptl_pt_index_t  pt_index_req,
               ptl_pt_index_t *pt_index)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlPTAlloc;

	buf->msg.PtlPTAlloc.ni_handle = ni_handle;
	buf->msg.PtlPTAlloc.options = options;
	buf->msg.PtlPTAlloc.eq_handle = eq_handle;
	buf->msg.PtlPTAlloc.pt_index_req = pt_index_req;

	transfer_msg(buf);

	err = buf->msg.ret;

	*pt_index = buf->msg.PtlPTAlloc.pt_index;

	ppebuf_release(buf);

	return err;
}

int PtlPTFree(ptl_handle_ni_t ni_handle,
              ptl_pt_index_t  pt_index)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlPTFree;

	buf->msg.PtlPTFree.ni_handle = ni_handle;
	buf->msg.PtlPTFree.pt_index = pt_index;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlPTDisable(ptl_handle_ni_t ni_handle,
                 ptl_pt_index_t  pt_index)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlPTDisable;

	buf->msg.PtlPTDisable.ni_handle = ni_handle;
	buf->msg.PtlPTDisable.pt_index = pt_index;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlPTEnable(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlPTEnable;

	buf->msg.PtlPTEnable.ni_handle = ni_handle;
	buf->msg.PtlPTEnable.pt_index = pt_index;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlGetUid(ptl_handle_ni_t ni_handle,
              ptl_uid_t      *uid)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlGetUid;

	buf->msg.PtlGetUid.ni_handle = ni_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	*uid = buf->msg.PtlGetUid.uid;

	ppebuf_release(buf);

	return err;
}

int PtlGetId(ptl_handle_ni_t ni_handle,
             ptl_process_t  *id)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlGetId;

	buf->msg.PtlGetId.ni_handle = ni_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	*id = buf->msg.PtlGetId.id;

	ppebuf_release(buf);

	return err;
}

int PtlGetPhysId(ptl_handle_ni_t ni_handle,
                 ptl_process_t  *id)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlGetPhysId;

	buf->msg.PtlGetPhysId.ni_handle = ni_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	*id = buf->msg.PtlGetPhysId.id;

	ppebuf_release(buf);

	return err;
}

int PtlMDBind(ptl_handle_ni_t  ni_handle,
              const ptl_md_t  *md,
              ptl_handle_md_t *md_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlMDBind;

	buf->msg.PtlMDBind.ni_handle = ni_handle;
	buf->msg.PtlMDBind.md = *md;

	transfer_msg(buf);

	err = buf->msg.ret;
	*md_handle = buf->msg.PtlMDBind.md_handle;

	ppebuf_release(buf);

	return err;
}

int PtlMDRelease(ptl_handle_md_t md_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlMDRelease;

	buf->msg.PtlMDRelease.md_handle = md_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlLEAppend(ptl_handle_ni_t  ni_handle,
                ptl_pt_index_t   pt_index,
                const ptl_le_t  *le,
                ptl_list_t       ptl_list,
                void            *user_ptr,
                ptl_handle_le_t *le_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlLEAppend;

	buf->msg.PtlLEAppend.ni_handle = ni_handle;
	buf->msg.PtlLEAppend.pt_index = pt_index;
	buf->msg.PtlLEAppend.le = *le;
	buf->msg.PtlLEAppend.ptl_list = ptl_list;
	buf->msg.PtlLEAppend.user_ptr = user_ptr;

	transfer_msg(buf);

	err = buf->msg.ret;
	*le_handle = buf->msg.PtlLEAppend.le_handle;

	ppebuf_release(buf);

	return err;
}

int PtlLEUnlink(ptl_handle_le_t le_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlLEUnlink;

	buf->msg.PtlLEUnlink.le_handle = le_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlLESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_le_t *le,
                ptl_search_op_t ptl_search_op,
                void           *user_ptr)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlLESearch;

	buf->msg.PtlLESearch.ni_handle = ni_handle;
	buf->msg.PtlLESearch.pt_index = pt_index;
	buf->msg.PtlLESearch.le = *le;
	buf->msg.PtlLESearch.ptl_search_op = ptl_search_op;
	buf->msg.PtlLESearch.user_ptr = user_ptr;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlMEAppend(ptl_handle_ni_t  ni_handle,
                ptl_pt_index_t   pt_index,
                const ptl_me_t  *me,
                ptl_list_t       ptl_list,
                void            *user_ptr,
                ptl_handle_me_t *me_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlMEAppend;

	buf->msg.PtlMEAppend.ni_handle = ni_handle;
	buf->msg.PtlMEAppend.pt_index = pt_index;
	buf->msg.PtlMEAppend.me = *me;
	buf->msg.PtlMEAppend.ptl_list = ptl_list;
	buf->msg.PtlMEAppend.user_ptr = user_ptr;

	transfer_msg(buf);

	err = buf->msg.ret;
	*me_handle = buf->msg.PtlMEAppend.me_handle;

	ppebuf_release(buf);

	return err;
}

int PtlMEUnlink(ptl_handle_me_t me_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlMEUnlink;

	buf->msg.PtlMEUnlink.me_handle = me_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlMESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_me_t *me,
                ptl_search_op_t ptl_search_op,
                void           *user_ptr)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlMESearch;

	buf->msg.PtlMESearch.ni_handle = ni_handle;
	buf->msg.PtlMESearch.pt_index = pt_index;
	buf->msg.PtlMESearch.me = *me;
	buf->msg.PtlMESearch.ptl_search_op = ptl_search_op;
	buf->msg.PtlMESearch.user_ptr = user_ptr;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

//todo: protect with lock
struct light_ct {
	struct list_head list;
	ptl_handle_ct_t ct_handle;
	struct xpmem_map ct_mapping;
	struct ct_info *info;
};
PTL_LIST_HEAD(CTs_list);

static struct light_ct *get_light_ct(ptl_handle_eq_t ct_handle)
{
	struct light_ct *ct;
	struct list_head *l;

	list_for_each(l, &CTs_list) {
		ct = list_entry(l, struct light_ct, list);
		if (ct->ct_handle == ct_handle)
			return ct;
	}

	return NULL;
}

int PtlCTAlloc(ptl_handle_ni_t  ni_handle,
               ptl_handle_ct_t *ct_handle)
{
	ppebuf_t *buf;
	int err;
	struct light_ct *ct;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	ct = calloc(1, sizeof(struct light_ct));
	if (!ct) {
		err = PTL_NO_SPACE;
		goto done;
	}

	buf->op = OP_PtlCTAlloc;

	buf->msg.PtlCTAlloc.ni_handle = ni_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	if (err == PTL_OK) {
		ct->ct_handle = buf->msg.PtlCTAlloc.ct_handle;
		ct->ct_mapping = buf->msg.PtlCTAlloc.ct_mapping;

		ct->info = map_segment(&ct->ct_mapping);
		if (!ct->info) {
			// call ctfree
			abort();
		}

		*ct_handle = buf->msg.PtlCTAlloc.ct_handle;

		/* Store the new CT locally. */
		list_add(&ct->list, &CTs_list);
	}

 done:
	ppebuf_release(buf);

	return err;
}

int PtlCTFree(ptl_handle_ct_t ct_handle)
{
	ppebuf_t *buf;
	int err;
	struct light_ct *ct = get_light_ct(ct_handle);

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlCTFree;

	buf->msg.PtlCTFree.ct_handle = ct_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	if (err == PTL_OK) {
		/* It's unclear whether unmapping after the segment has been
		 * destroyed on the PPE is an error. */
		unmap_segment(&ct->ct_mapping);
		list_del(&ct->list);
		free(ct);
	}

	return err;
}

int PtlCTCancelTriggered(ptl_handle_ct_t ct_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlCTCancelTriggered;

	buf->msg.PtlCTCancelTriggered.ct_handle = ct_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlCTGet(ptl_handle_ct_t ct_handle,
             ptl_ct_event_t *event)
{
	const struct light_ct *ct;
	int err;

#ifndef NO_ARG_VALIDATION
	if (!ppe.ppe_comm_pad)
		return PTL_NO_INIT;
#endif

	ct = get_light_ct(ct_handle);
	if (ct) {
		*event = ct->info->event;
		err = PTL_OK;
	} else {
		err = PTL_ARG_INVALID;
	}

	return err;
}

int PtlCTWait(ptl_handle_ct_t ct_handle,
              ptl_size_t      test,
              ptl_ct_event_t *event)
{
	const struct light_ct *ct;
	int err;

#ifndef NO_ARG_VALIDATION
	if (!ppe.ppe_comm_pad)
		return PTL_NO_INIT;
#endif

	ct = get_light_ct(ct_handle);
	if (ct)
		err = PtlCTWait_work(ct->info, test, event);
	else
		err = PTL_ARG_INVALID;

	return err;
}

int PtlCTPoll(const ptl_handle_ct_t *ct_handles,
              const ptl_size_t      *tests,
              unsigned int           size,
              ptl_time_t             timeout,
              ptl_ct_event_t        *event,
              unsigned int          *which)
{
	int err;
	int i;
	struct ct_info **cts_info = NULL;

#ifndef NO_ARG_VALIDATION
	if (!ppe.ppe_comm_pad)
		return PTL_NO_INIT;
#endif

	if (size == 0) {
		err = PTL_ARG_INVALID;
		goto done;
	}

	cts_info = malloc(size * sizeof(struct ct_info));
	if (!cts_info) {
		err = PTL_NO_SPACE;
		goto done;
	}

	for (i = 0; i < size; i++) {
		struct light_ct *ct = get_light_ct(ct_handles[i]);
		if (!ct) {
			err = PTL_ARG_INVALID;
			goto done;
		}
		cts_info[i] = ct->info;
	}

	err = PtlCTPoll_work(cts_info, tests, size, timeout, event, which);

 done:
	if (cts_info)
		free(cts_info);

	return err;
}

int PtlCTSet(ptl_handle_ct_t ct_handle,
             ptl_ct_event_t  new_ct)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlCTSet;

	buf->msg.PtlCTSet.ct_handle = ct_handle;
	buf->msg.PtlCTSet.new_ct = new_ct;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}


int PtlCTInc(ptl_handle_ct_t ct_handle,
             ptl_ct_event_t  increment)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlCTInc;

	buf->msg.PtlCTInc.ct_handle = ct_handle;
	buf->msg.PtlCTInc.increment = increment;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlPut(ptl_handle_md_t  md_handle,
           ptl_size_t       local_offset,
           ptl_size_t       length,
           ptl_ack_req_t    ack_req,
           ptl_process_t    target_id,
           ptl_pt_index_t   pt_index,
           ptl_match_bits_t match_bits,
           ptl_size_t       remote_offset,
           void            *user_ptr,
           ptl_hdr_data_t   hdr_data)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlPut;

	buf->msg.PtlPut.md_handle = md_handle;
	buf->msg.PtlPut.local_offset = local_offset;
	buf->msg.PtlPut.length = length;
	buf->msg.PtlPut.ack_req = ack_req;
	buf->msg.PtlPut.target_id = target_id;
	buf->msg.PtlPut.pt_index = pt_index;
	buf->msg.PtlPut.match_bits = match_bits;
	buf->msg.PtlPut.remote_offset = remote_offset;
	buf->msg.PtlPut.user_ptr = user_ptr;
	buf->msg.PtlPut.hdr_data = hdr_data;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlGet(ptl_handle_md_t  md_handle,
           ptl_size_t       local_offset,
           ptl_size_t       length,
           ptl_process_t    target_id,
           ptl_pt_index_t   pt_index,
           ptl_match_bits_t match_bits,
           ptl_size_t       remote_offset,
           void            *user_ptr)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlGet;

	buf->msg.PtlGet.md_handle = md_handle;
	buf->msg.PtlGet.local_offset = local_offset;
	buf->msg.PtlGet.length = length;
	buf->msg.PtlGet.target_id = target_id;
	buf->msg.PtlGet.pt_index = pt_index;
	buf->msg.PtlGet.match_bits = match_bits;
	buf->msg.PtlGet.remote_offset = remote_offset;
	buf->msg.PtlGet.user_ptr = user_ptr;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlAtomic(ptl_handle_md_t  md_handle,
              ptl_size_t       local_offset,
              ptl_size_t       length,
              ptl_ack_req_t    ack_req,
              ptl_process_t    target_id,
              ptl_pt_index_t   pt_index,
              ptl_match_bits_t match_bits,
              ptl_size_t       remote_offset,
              void            *user_ptr,
              ptl_hdr_data_t   hdr_data,
              ptl_op_t         operation,
              ptl_datatype_t   datatype)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlAtomic;

	buf->msg.PtlAtomic.md_handle = md_handle;
	buf->msg.PtlAtomic.local_offset = local_offset;
	buf->msg.PtlAtomic.length = length;
	buf->msg.PtlAtomic.ack_req = ack_req;
	buf->msg.PtlAtomic.target_id = target_id;
	buf->msg.PtlAtomic.pt_index = pt_index;
	buf->msg.PtlAtomic.match_bits = match_bits;
	buf->msg.PtlAtomic.remote_offset = remote_offset;
	buf->msg.PtlAtomic.user_ptr = user_ptr;
	buf->msg.PtlAtomic.hdr_data = hdr_data;
	buf->msg.PtlAtomic.operation = operation;
	buf->msg.PtlAtomic.datatype = datatype;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlFetchAtomic(ptl_handle_md_t  get_md_handle,
                   ptl_size_t       local_get_offset,
                   ptl_handle_md_t  put_md_handle,
                   ptl_size_t       local_put_offset,
                   ptl_size_t       length,
                   ptl_process_t    target_id,
                   ptl_pt_index_t   pt_index,
                   ptl_match_bits_t match_bits,
                   ptl_size_t       remote_offset,
                   void            *user_ptr,
                   ptl_hdr_data_t   hdr_data,
                   ptl_op_t         operation,
                   ptl_datatype_t   datatype)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlFetchAtomic;

	buf->msg.PtlFetchAtomic.get_md_handle = get_md_handle;
	buf->msg.PtlFetchAtomic.local_get_offset = local_get_offset;
	buf->msg.PtlFetchAtomic.put_md_handle = put_md_handle;
	buf->msg.PtlFetchAtomic.local_put_offset = local_put_offset;
	buf->msg.PtlFetchAtomic.length = length;
	buf->msg.PtlFetchAtomic.target_id = target_id;
	buf->msg.PtlFetchAtomic.pt_index = pt_index;
	buf->msg.PtlFetchAtomic.match_bits = match_bits;
	buf->msg.PtlFetchAtomic.remote_offset = remote_offset;
	buf->msg.PtlFetchAtomic.user_ptr = user_ptr;
	buf->msg.PtlFetchAtomic.hdr_data = hdr_data;
	buf->msg.PtlFetchAtomic.operation = operation;
	buf->msg.PtlFetchAtomic.datatype = datatype;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlSwap(ptl_handle_md_t  get_md_handle,
            ptl_size_t       local_get_offset,
            ptl_handle_md_t  put_md_handle,
            ptl_size_t       local_put_offset,
            ptl_size_t       length,
            ptl_process_t    target_id,
            ptl_pt_index_t   pt_index,
            ptl_match_bits_t match_bits,
            ptl_size_t       remote_offset,
            void            *user_ptr,
            ptl_hdr_data_t   hdr_data,
            const void      *operand,
            ptl_op_t         operation,
            ptl_datatype_t   datatype)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlSwap;

	buf->msg.PtlSwap.get_md_handle = get_md_handle;
	buf->msg.PtlSwap.local_get_offset = local_get_offset;
	buf->msg.PtlSwap.put_md_handle = put_md_handle;
	buf->msg.PtlSwap.local_put_offset = local_put_offset;
	buf->msg.PtlSwap.length = length;
	buf->msg.PtlSwap.target_id = target_id;
	buf->msg.PtlSwap.pt_index = pt_index;
	buf->msg.PtlSwap.match_bits = match_bits;
	buf->msg.PtlSwap.remote_offset = remote_offset;
	buf->msg.PtlSwap.user_ptr = user_ptr;
	buf->msg.PtlSwap.hdr_data = hdr_data;
	buf->msg.PtlSwap.operand = operand;
	buf->msg.PtlSwap.operation = operation;
	buf->msg.PtlSwap.datatype = datatype;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlAtomicSync(void)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlAtomicSync;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

//todo: protect with lock
struct light_eq {
	struct list_head list;
	ptl_handle_eq_t eq_handle;
	struct xpmem_map eqe_list_map;
	struct eqe_list *eqe_list;
};
PTL_LIST_HEAD(EQs_list);

static struct light_eq *get_light_eq(ptl_handle_eq_t eq_handle)
{
	struct light_eq *eq;
	struct list_head *l;

	list_for_each(l, &EQs_list) {
		eq = list_entry(l, struct light_eq, list);
		if (eq->eq_handle == eq_handle)
			return eq;
	}

	return NULL;
}

int PtlEQAlloc(ptl_handle_ni_t  ni_handle,
               ptl_size_t       count,
               ptl_handle_eq_t *eq_handle)
{
	ppebuf_t *buf;
	int err;
	struct light_eq *eq;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	eq = calloc(1, sizeof(struct light_eq));
	if (!eq) {
		err = PTL_NO_SPACE;
		goto done;
	}

	buf->op = OP_PtlEQAlloc;

	buf->msg.PtlEQAlloc.ni_handle = ni_handle;
	buf->msg.PtlEQAlloc.count = count;

	transfer_msg(buf);

	err = buf->msg.ret;

	*eq_handle = buf->msg.PtlEQAlloc.eq_handle;

	if (err == PTL_OK) {
		struct eqe_list *local_eqe_list;

		eq->eqe_list_map = buf->msg.PtlEQAlloc.eqe_list;
		local_eqe_list = map_segment(&eq->eqe_list_map);

		if (local_eqe_list == NULL) {
			// todo: call eqfree and return an error
			abort();
		}

		/* Store the new EQ locally. */
		eq->eq_handle = *eq_handle;
		eq->eqe_list = local_eqe_list;
		list_add(&eq->list, &EQs_list);
	}

 done:
	ppebuf_release(buf);

	return err;
}

int PtlEQFree(ptl_handle_eq_t eq_handle)
{
	ppebuf_t *buf;
	int err;
	struct light_eq *eq;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	eq = get_light_eq(eq_handle);
	if (!eq) {
		err = PTL_ARG_INVALID;
		goto done;
	}

	buf->op = OP_PtlEQFree;

	buf->msg.PtlEQFree.eq_handle = eq_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	if (err == PTL_OK) {
		/* It's unclear whether unmapping after the segment has been
		 * destroyed on the PPE is an error. */
		unmap_segment(&eq->eqe_list_map);
		list_del(&eq->list);
		free(eq);
	}

 done:
	ppebuf_release(buf);

	return err;
}

int PtlEQGet(ptl_handle_eq_t eq_handle,
             ptl_event_t    *event)
{
	const struct light_eq *eq;
	int err;

#ifndef NO_ARG_VALIDATION
	if (!ppe.ppe_comm_pad)
		return PTL_NO_INIT;
#endif

	eq = get_light_eq(eq_handle);
	if (eq) {
		err = PtlEQGet_work(eq->eqe_list, event);
	} else {
		err = PTL_ARG_INVALID;
	}

	return err;
}

int PtlEQWait(ptl_handle_eq_t eq_handle,
              ptl_event_t    *event)
{
	const struct light_eq *eq;
	int err;

#ifndef NO_ARG_VALIDATION
	if (!ppe.ppe_comm_pad)
		return PTL_NO_INIT;
#endif

	eq = get_light_eq(eq_handle);
	if (eq)
		err = PtlEQWait_work(eq->eqe_list, event);
	else
		err = PTL_ARG_INVALID;

	return err;
}

int PtlEQPoll(const ptl_handle_eq_t *eq_handles,
              unsigned int           size,
              ptl_time_t             timeout,
              ptl_event_t           *event,
              unsigned int          *which)
{
	int err;
	int i;
	struct eqe_list **eqes_list = NULL;

#ifndef NO_ARG_VALIDATION
	if (!ppe.ppe_comm_pad)
		return PTL_NO_INIT;
#endif

	if (size == 0) {
		err = PTL_ARG_INVALID;
		goto done;
	}

	eqes_list = malloc(size*sizeof(struct eqe_list));
	if (!eqes_list) {
		err = PTL_NO_SPACE;
		goto done;
	}

	for (i = 0; i < size; i++) {
		struct light_eq *eq = get_light_eq(eq_handles[i]);
		if (!eq) {
			err = PTL_ARG_INVALID;
			goto done;
		}
		eqes_list[i] = eq->eqe_list;
	}

	err = PtlEQPoll_work(eqes_list, size,
						 timeout, event, which);

 done:
	if (eqes_list)
		free(eqes_list);

	return err;
}

int PtlTriggeredPut(ptl_handle_md_t  md_handle,
                    ptl_size_t       local_offset,
                    ptl_size_t       length,
                    ptl_ack_req_t    ack_req,
                    ptl_process_t    target_id,
                    ptl_pt_index_t   pt_index,
                    ptl_match_bits_t match_bits,
                    ptl_size_t       remote_offset,
                    void            *user_ptr,
                    ptl_hdr_data_t   hdr_data,
                    ptl_handle_ct_t  trig_ct_handle,
                    ptl_size_t       threshold)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlTriggeredPut;

	buf->msg.PtlTriggeredPut.md_handle = md_handle;
	buf->msg.PtlTriggeredPut.local_offset = local_offset;
	buf->msg.PtlTriggeredPut.length = length;
	buf->msg.PtlTriggeredPut.ack_req = ack_req;
	buf->msg.PtlTriggeredPut.target_id = target_id;
	buf->msg.PtlTriggeredPut.pt_index = pt_index;
	buf->msg.PtlTriggeredPut.match_bits = match_bits;
	buf->msg.PtlTriggeredPut.remote_offset = remote_offset;
	buf->msg.PtlTriggeredPut.user_ptr = user_ptr;
	buf->msg.PtlTriggeredPut.hdr_data = hdr_data;
	buf->msg.PtlTriggeredPut.trig_ct_handle = trig_ct_handle;
	buf->msg.PtlTriggeredPut.threshold = threshold;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlTriggeredGet(ptl_handle_md_t  md_handle,
                    ptl_size_t       local_offset,
                    ptl_size_t       length,
                    ptl_process_t    target_id,
                    ptl_pt_index_t   pt_index,
                    ptl_match_bits_t match_bits,
                    ptl_size_t       remote_offset,
                    void            *user_ptr,
                    ptl_handle_ct_t  trig_ct_handle,
                    ptl_size_t       threshold)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlTriggeredGet;

	buf->msg.PtlTriggeredGet.md_handle = md_handle;
	buf->msg.PtlTriggeredGet.local_offset = local_offset;
	buf->msg.PtlTriggeredGet.length = length;
	buf->msg.PtlTriggeredGet.target_id = target_id;
	buf->msg.PtlTriggeredGet.pt_index = pt_index;
	buf->msg.PtlTriggeredGet.match_bits = match_bits;
	buf->msg.PtlTriggeredGet.remote_offset = remote_offset;
	buf->msg.PtlTriggeredGet.user_ptr = user_ptr;
	buf->msg.PtlTriggeredGet.trig_ct_handle = trig_ct_handle;
	buf->msg.PtlTriggeredGet.threshold = threshold;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlTriggeredAtomic(ptl_handle_md_t  md_handle,
                       ptl_size_t       local_offset,
                       ptl_size_t       length,
                       ptl_ack_req_t    ack_req,
                       ptl_process_t    target_id,
                       ptl_pt_index_t   pt_index,
                       ptl_match_bits_t match_bits,
                       ptl_size_t       remote_offset,
                       void            *user_ptr,
                       ptl_hdr_data_t   hdr_data,
                       ptl_op_t         operation,
                       ptl_datatype_t   datatype,
                       ptl_handle_ct_t  trig_ct_handle,
                       ptl_size_t       threshold)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlTriggeredAtomic;

	buf->msg.PtlTriggeredAtomic.md_handle = md_handle;
	buf->msg.PtlTriggeredAtomic.local_offset = local_offset;
	buf->msg.PtlTriggeredAtomic.length = length;
	buf->msg.PtlTriggeredAtomic.ack_req = ack_req;
	buf->msg.PtlTriggeredAtomic.target_id = target_id;
	buf->msg.PtlTriggeredAtomic.pt_index = pt_index;
	buf->msg.PtlTriggeredAtomic.match_bits = match_bits;
	buf->msg.PtlTriggeredAtomic.remote_offset = remote_offset;
	buf->msg.PtlTriggeredAtomic.user_ptr = user_ptr;
	buf->msg.PtlTriggeredAtomic.hdr_data = hdr_data;
	buf->msg.PtlTriggeredAtomic.operation = operation;
	buf->msg.PtlTriggeredAtomic.datatype = datatype;
	buf->msg.PtlTriggeredAtomic.trig_ct_handle = trig_ct_handle;
	buf->msg.PtlTriggeredAtomic.threshold = threshold;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlTriggeredFetchAtomic(ptl_handle_md_t  get_md_handle,
                            ptl_size_t       local_get_offset,
                            ptl_handle_md_t  put_md_handle,
                            ptl_size_t       local_put_offset,
                            ptl_size_t       length,
                            ptl_process_t    target_id,
                            ptl_pt_index_t   pt_index,
                            ptl_match_bits_t match_bits,
                            ptl_size_t       remote_offset,
                            void            *user_ptr,
                            ptl_hdr_data_t   hdr_data,
                            ptl_op_t         operation,
                            ptl_datatype_t   datatype,
                            ptl_handle_ct_t  trig_ct_handle,
                            ptl_size_t       threshold)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlTriggeredFetchAtomic;

	buf->msg.PtlTriggeredFetchAtomic.get_md_handle = get_md_handle;
	buf->msg.PtlTriggeredFetchAtomic.local_get_offset = local_get_offset;
	buf->msg.PtlTriggeredFetchAtomic.put_md_handle = put_md_handle;
	buf->msg.PtlTriggeredFetchAtomic.local_put_offset = local_put_offset;
	buf->msg.PtlTriggeredFetchAtomic.length = length;
	buf->msg.PtlTriggeredFetchAtomic.target_id = target_id;
	buf->msg.PtlTriggeredFetchAtomic.pt_index = pt_index;
	buf->msg.PtlTriggeredFetchAtomic.match_bits = match_bits;
	buf->msg.PtlTriggeredFetchAtomic.remote_offset = remote_offset;
	buf->msg.PtlTriggeredFetchAtomic.user_ptr = user_ptr;
	buf->msg.PtlTriggeredFetchAtomic.hdr_data = hdr_data;
	buf->msg.PtlTriggeredFetchAtomic.operation = operation;
	buf->msg.PtlTriggeredFetchAtomic.datatype = datatype;
	buf->msg.PtlTriggeredFetchAtomic.trig_ct_handle = trig_ct_handle;
	buf->msg.PtlTriggeredFetchAtomic.threshold = threshold;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlTriggeredSwap(ptl_handle_md_t  get_md_handle,
                     ptl_size_t       local_get_offset,
                     ptl_handle_md_t  put_md_handle,
                     ptl_size_t       local_put_offset,
                     ptl_size_t       length,
                     ptl_process_t    target_id,
                     ptl_pt_index_t   pt_index,
                     ptl_match_bits_t match_bits,
                     ptl_size_t       remote_offset,
                     void            *user_ptr,
                     ptl_hdr_data_t   hdr_data,
                     const void      *operand,
                     ptl_op_t         operation,
                     ptl_datatype_t   datatype,
                     ptl_handle_ct_t  trig_ct_handle,
                     ptl_size_t       threshold)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlTriggeredSwap;

	buf->msg.PtlTriggeredSwap.get_md_handle = get_md_handle;
	buf->msg.PtlTriggeredSwap.local_get_offset = local_get_offset;
	buf->msg.PtlTriggeredSwap.put_md_handle = put_md_handle;
	buf->msg.PtlTriggeredSwap.local_put_offset = local_put_offset;
	buf->msg.PtlTriggeredSwap.length = length;
	buf->msg.PtlTriggeredSwap.target_id = target_id;
	buf->msg.PtlTriggeredSwap.pt_index = pt_index;
	buf->msg.PtlTriggeredSwap.match_bits = match_bits;
	buf->msg.PtlTriggeredSwap.remote_offset = remote_offset;
	buf->msg.PtlTriggeredSwap.user_ptr = user_ptr;
	buf->msg.PtlTriggeredSwap.hdr_data = hdr_data;
	buf->msg.PtlTriggeredSwap.operand = operand;
	buf->msg.PtlTriggeredSwap.operation = operation;
	buf->msg.PtlTriggeredSwap.datatype = datatype;
	buf->msg.PtlTriggeredSwap.trig_ct_handle = trig_ct_handle;
	buf->msg.PtlTriggeredSwap.threshold = threshold;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlTriggeredCTInc(ptl_handle_ct_t ct_handle,
                      ptl_ct_event_t  increment,
                      ptl_handle_ct_t trig_ct_handle,
                      ptl_size_t      threshold)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlTriggeredCTInc;

	buf->msg.PtlTriggeredCTInc.ct_handle = ct_handle;
	buf->msg.PtlTriggeredCTInc.increment = increment;
	buf->msg.PtlTriggeredCTInc.trig_ct_handle = trig_ct_handle;
	buf->msg.PtlTriggeredCTInc.threshold = threshold;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlTriggeredCTSet(ptl_handle_ct_t ct_handle,
                      ptl_ct_event_t  new_ct,
                      ptl_handle_ct_t trig_ct_handle,
                      ptl_size_t      threshold)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlTriggeredCTSet;

	buf->msg.PtlTriggeredCTSet.ct_handle = ct_handle;
	buf->msg.PtlTriggeredCTSet.new_ct = new_ct;
	buf->msg.PtlTriggeredCTSet.trig_ct_handle = trig_ct_handle;
	buf->msg.PtlTriggeredCTSet.threshold = threshold;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlStartBundle(ptl_handle_ni_t ni_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlStartBundle;

	buf->msg.PtlStartBundle.ni_handle = ni_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

int PtlEndBundle(ptl_handle_ni_t ni_handle)
{
	ppebuf_t *buf;
	int err;

	if ((err = ppebuf_alloc(&buf))) {
		WARN();
		return err;
	}

	buf->op = OP_PtlEndBundle;

	buf->msg.PtlEndBundle.ni_handle = ni_handle;

	transfer_msg(buf);

	err = buf->msg.ret;

	ppebuf_release(buf);

	return err;
}

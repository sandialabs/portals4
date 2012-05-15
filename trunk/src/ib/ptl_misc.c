/*
 * ptl_misc.c
 *
 * Various parts not belonging elsewhere.
 */

#include "ptl_loc.h"

/* Internal debug tuning variables. */
int debug;
int ptl_log_level;

unsigned long pagesize;
unsigned int linesize;

static pthread_mutex_t per_proc_gbl_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Various initalization that must be done once. */
int misc_init_once(void)
{
	int err;

	init_param();
	debug = get_param(PTL_DEBUG);
	ptl_log_level = get_param(PTL_LOG_LEVEL);

	/* init the index service */
	err = index_init();
	if (err)
		return err;

	pagesize = sysconf(_SC_PAGESIZE);
#ifdef _SC_LEVEL1_DCACHE_LINESIZE
	linesize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#else
	linesize = 64;
#endif

	return PTL_OK;
}

int _PtlInit(gbl_t *gbl)
{
	int ret;

	ret = pthread_mutex_lock(&per_proc_gbl_mutex);
	if (ret) {
		ptl_warn("unable to acquire proc_gbl mutex\n");
		ret = PTL_FAIL;
		goto err0;
	}

	if (gbl->finalized) {
		ptl_warn("Portals was finalized\n");
		ret = PTL_FAIL;
		goto err1;
	}

	/* if first call to PtlInit do real initialization */
	if (gbl->ref_cnt == 0) {
		ref_set(&gbl->ref, 1);

		ret = gbl_init(gbl);
		if (ret != PTL_OK) {
			ref_set(&gbl->ref, 0);
			goto err1;
		}
	}

	gbl->ref_cnt++;
	pthread_mutex_unlock(&per_proc_gbl_mutex);

	return PTL_OK;

err1:
	pthread_mutex_unlock(&per_proc_gbl_mutex);
err0:
	return ret;
}

void _PtlFini(gbl_t *gbl)
{
	int ret;

	ret = pthread_mutex_lock(&per_proc_gbl_mutex);
	if (ret) {
		ptl_warn("unable to acquire proc_gbl mutex\n");
		abort();
		goto err0;
	}

	if (gbl->finalized == 1) {
		ptl_warn("Portals already finalized\n");
		goto err0;
	}

	/* this would be a bug */
	if (gbl->ref_cnt == 0) {
		ptl_warn("ref_cnt already 0 ?!!\n");
		goto err1;
	}

	/* note the order is significant here
	   gbl->ref_cnt != 0 implies that the
	   spinlock in gbl->ref has been init'ed
	   so ref_set must come before the initial
	   ref_cnt++ and ref_put must come after
	   the last ref_cnt-- */
	gbl->ref_cnt--;

	if (gbl->ref_cnt == 0) {
		gbl->finalized = 1;
		ref_put(&gbl->ref, gbl_release);	/* matches ref_set */
	}

	pthread_mutex_unlock(&per_proc_gbl_mutex);

	return;

err1:
	pthread_mutex_unlock(&per_proc_gbl_mutex);
err0:
	return;
}

/* can return
PTL_OK
PTL_FAIL
*/
int PtlHandleIsEqual(ptl_handle_any_t handle1,
		     ptl_handle_any_t handle2)
{
	return (handle1 == handle2) ? PTL_OK : PTL_FAIL;
}

#if WITH_PPE
/* Given a memory segment, create a mapping for XPMEM, and returns the
 * segid and the offset of the buffer. Return PTL_OK on success. */
int create_mapping(const void *addr_in, size_t length, struct xpmem_map *mapping)
{
	void *addr;

	/* Align the address to a page boundary. */
	addr = (void *)(((uintptr_t)addr_in) & ~(pagesize-1));
	mapping->offset = addr_in - addr;
	mapping->source_addr = addr_in;

	/* Adjust the size to comprise full pages. */
	mapping->size = length;
	length += mapping->offset;
	length = (length + pagesize-1) & ~(pagesize-1);

	mapping->segid = xpmem_make(addr, length,	
					   XPMEM_PERMIT_MODE, (void *)0600);

	return mapping->segid == -1 ? PTL_ARG_INVALID : PTL_OK;
}

/* Delete an existing mapping. */
void delete_mapping(struct xpmem_map *mapping)
{
	 xpmem_remove(mapping->segid);
}

/* Attach to an XPMEM segment. */
void *map_segment(struct xpmem_map *mapping)
{
	mapping->addr.apid = xpmem_get(mapping->segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, NULL);
	if (mapping->addr.apid == -1) {
		/* That's bad. It's not possible to recover. */
		//todo: proper shutdown.
		WARN();
		abort();
		return NULL;
	}

	/* Hack. When addr.offset is non-zero, xpmem_attach() always fail. So fix the ptr afterwards. */
	mapping->addr.offset = 0;
	mapping->ptr = xpmem_attach(mapping->addr, mapping->size+mapping->offset, NULL);
	if (mapping->ptr == (void *)-1) {
		WARN();
		//todo: proper shutdown.
		abort();
		return NULL;
	}

	mapping->ptr += mapping->offset;

	return mapping->ptr;
}

/* Detach from an XPMEM segment. */
void unmap_segment(struct xpmem_map *mapping)
{
	if (!(xpmem_detach(mapping->ptr)))
		return;

	xpmem_release(mapping->addr.apid);
}
#endif

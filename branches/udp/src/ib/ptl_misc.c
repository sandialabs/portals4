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
     if (0 == linesize) linesize = 64;
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

/* can return */
int PtlHandleIsEqual(ptl_handle_any_t handle1,
		     ptl_handle_any_t handle2)
{
	return (handle1 == handle2);
}

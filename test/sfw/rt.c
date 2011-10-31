#include "ptl_test.h"
#include "support/support.h"

int ompi_rt_init(struct node_info *info)
{
	int errs = 0;

	libtest_init();

	info->rank = libtest_get_rank();
	info->map_size = libtest_get_size();

	if (info->ni_handle != PTL_INVALID_HANDLE) {
		if (!info->desired_map_ptr) {
			info->desired_map_ptr = libtest_get_mapping();
			if (!info->desired_map_ptr) {
				errs ++;
			}
		}
	}

	return errs;
}

int ompi_rt_fini(struct node_info *info)
{
	if (info->desired_map_ptr) {
		info->desired_map_ptr = NULL;
	}

	return 0;
}

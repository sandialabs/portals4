/*
 * ptl_index.c
 */

#include "ptl_loc.h"

/* Maximum number of stored objects ever. */
#define MAX_INDEX	(256*1024)

static atomic_t next_index;
static void **index_map;

static int index_is_init;

int index_init(void)
{
	index_map = calloc(MAX_INDEX, sizeof(void *));
	if (!index_map) {
		return PTL_NO_SPACE;
	}

	atomic_set(&next_index, 0);

	index_is_init = 1;

	return PTL_OK;
}

void index_fini(void)
{
	index_is_init = 0;

	free(index_map);
}

int index_get(obj_t *obj, unsigned int *index_p)
{
	unsigned int index;

#ifndef NO_ARG_VALIDATION
	if (!index_is_init) {
		WARN();
		return PTL_FAIL;
	}
#endif

	index = atomic_inc(&next_index);

	index_map[index] = obj;

	*index_p = index;

	return PTL_OK;
}

int index_lookup(unsigned int index, obj_t **obj_p)
{
#ifndef NO_ARG_VALIDATION
	if (!index_is_init) {
		WARN();
		return PTL_FAIL;
	}
#endif

	if (index >= MAX_INDEX) {
		WARN();
		return PTL_FAIL;
	}

	if (index_map[index]) {
		*obj_p = index_map[index];
		return PTL_OK;
	} else {
		return PTL_FAIL;
	}
}

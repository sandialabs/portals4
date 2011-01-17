/*
 * ptl_maps.c - code for checking addresses and ranges
 */

/*
 * this is for debugging only, not performance critical since
 * it will get turned off in production
 */

/* TODO
 *	1. allocate array dynamically
 *	2. recompute map list when new
 *	   memory is allocated, mmap'ed
 *	   or destroyed. How?
 */

#include "ptl_loc.h"

struct map_entry {
	unsigned long	from;
	unsigned long	to;
};

#define MAX_MAPS	(128)

static struct map_entry mem_maps[MAX_MAPS];
static int num_maps;
pthread_mutex_t map_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * read /proc/self/maps to find current memory map and build a list
 * of writable memory segments
 */
int get_maps(void)
{
	FILE *fp;
	char line[128];
	unsigned long from;
	unsigned long to;
	char flags[8];
	int n;

	pthread_mutex_lock(&map_mutex);
	fp = fopen("/proc/self/maps", "r");

	if (!fp) {
		pthread_mutex_unlock(&map_mutex);
		printf("unable to read /proc/self/maps\n");
		return PTL_FAIL;
	}

	num_maps = 0;

	while (fgets(line, sizeof(line), fp)) {
		n = sscanf(line, "%lx-%lx %s",
			&from, &to, flags);
		if (flags[1] == 'w') {
			mem_maps[num_maps].from = from;
			mem_maps[num_maps].to = to;
			num_maps++;
		}

		if (num_maps >= MAX_MAPS) {
			printf("TOO MANY MAPS\n");
			break;
		}
	}
	fclose(fp);
	pthread_mutex_unlock(&map_mutex);

	return PTL_OK;
}

/* check to see if address range fits into
 * one of the memory segments */
int check_range(void *addr, unsigned long length)
{
	int i;
	unsigned long start = (unsigned long)addr;

	pthread_mutex_lock(&map_mutex);
	if (start < mem_maps[0].from ||
	    (start + length) > mem_maps[num_maps-1].to) {
		pthread_mutex_unlock(&map_mutex);
		return PTL_ARG_INVALID;
	}

	for (i = 0; i < num_maps; i++) {
		if (mem_maps[i].from <= start &&
		    mem_maps[i].to >= (start+length)) {
			pthread_mutex_unlock(&map_mutex);
			return PTL_OK;
		}
	}
	pthread_mutex_unlock(&map_mutex);

	return PTL_ARG_INVALID;
}

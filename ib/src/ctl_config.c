#include "ctl.h"

int create_shared_memory(struct p4oibd_config *conf)
{
	const int psize = getpagesize();
	const int pmask = psize - 1;
	size_t size;
	size_t total_size;
	struct shared_config sc;
	unsigned char *m;
	int ret;

	memset(&sc, 0, sizeof(struct shared_config));

	/* Reserve space for the header. */
	size = sizeof(struct shared_config);
	size = (size + pmask) & ~pmask;
	total_size = size;

	/* Compute the sizes and offsets. */
	size = sizeof(struct rank_table) +
		conf->nranks * sizeof(struct rank_entry);
	size = (size + pmask) & ~pmask;
	sc.rank_table_offset = total_size;
	sc.rank_table_size = size;
	total_size += size;

	/* Create the file and copy the data there. */
	sprintf(conf->shmem.filename, "/tmp/p4oibd-%d-shm-XXXXXX", getpid());
	conf->shmem.fd = mkstemp(conf->shmem.filename);

	if (conf->shmem.fd == -1)
		return 1;

	ret = ftruncate(conf->shmem.fd, total_size);
	if (ret < 0) {
		printf("ftruncate failed, ret = %d\n", ret);
		close(conf->shmem.fd);
		conf->shmem.fd = -1;
		return 1;
	}

	m = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		conf->shmem.fd, 0);
	if (m == MAP_FAILED)
		return 1;
	memset(m, 0, total_size);

	conf->shmem.m = (struct shared_config *)m;
	conf->shmem.filesize = total_size;

	/* Copy the header and the tables into the shared memory. */
	memcpy(m, &sc, sizeof(sc));

	conf->master_rank_table = (struct rank_table *)(m + sc.rank_table_offset);

	return 0;
}

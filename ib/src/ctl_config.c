#include "ctl.h"

/* Load a NID table. The format is CSV. The arguments are NID ; IP ; */
int load_nid_table(const char *name)
{
	FILE *f;
	char *str;
	char line[1000];
	const int table_elem_wm = 10;
	int nb_alloc;
	struct nid_entry entry;
	struct nid_table *table;
	unsigned int l;

	table = malloc(sizeof(struct nid_table));
	table->size = 0;
	nb_alloc = 0;

	f = fopen(name, "r");
	if (!f)
		return 1;

	while(fgets(line, sizeof(line), f)) {

		if (line[0] == '#')
			continue;

		/* retrieve NID */
		str = strtok(line, ";");
		entry.nid = atoi(str);

		/* retrieve IP */
		/* Trim spaces before using inet_pton. */
		str = strtok(NULL, ";");
		while (isspace(*str))
			str ++;

		l = strlen(str);
		while (l && isspace(str[l-1])) {
			l--;
			str[l] = 0;
		}

		if (inet_pton(AF_INET, str, &entry.addr_in.sin_addr) == 1) {
			entry.addr_in.sin_family = AF_INET;
		}
		else if (inet_pton(AF_INET6, str,
		    &entry.addr_in6.sin6_addr) == 1) {
			entry.addr_in6.sin6_family = AF_INET6;
		}
		else {
			return 1;
		}

		if (table->size == nb_alloc) {
			nb_alloc += table_elem_wm;
			table = realloc(table, sizeof(struct nid_table) + 
					nb_alloc * sizeof(struct nid_entry));
			if (!table)
				return 1;
		}

		table->elem[table->size] = entry;
		table->size ++;
	}

	fclose(f);

	conf.nid_table = table;

	return 0;
}


/* Load a RANK table. The format is CSV. The arguments are RANK ; NID ; PID. */
int load_rank_table(const char *name)
{
	FILE *f;
	char *str;
	char line[1000];
	const int table_elem_wm = 10;
	int nb_alloc;
	struct rank_entry entry;
	struct rank_table *table;

	table = malloc(sizeof(struct rank_table));
	table->size = 0;
	nb_alloc = 0;

	f = fopen(name, "r");
	if (!f)
		return 1;

	while(fgets(line, sizeof(line), f)) {

		if (line[0] == '#')
			continue;

		memset(&entry, 0, sizeof(struct rank_entry));

		/* retrieve RANK */
		str = strtok(line, ";");
		entry.rank = atoi(str);

		/* retrieve NID */
		str = strtok(NULL, ";");
		entry.nid = atoi(str);

		/* Retrieve PID */
		str = strtok(NULL, ";");
		entry.pid = atoi(str);
		
		if (table->size == nb_alloc) {
			nb_alloc += table_elem_wm;
			table = realloc(table, sizeof(struct rank_table) + 
					nb_alloc * sizeof(struct rank_entry));
			if (!table)
				return 1;
		}

		table->elem[table->size] = entry;
		table->size ++;
	}

	fclose(f);

	conf.rank_table = table;

	return 0;
}

int create_shared_memory(void)
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
	size = sizeof(struct rank_table) + conf.rank_table->size *
			sizeof(struct rank_entry);
	size = (size + pmask) & ~pmask;
	sc.rank_table_offset = total_size;
	sc.rank_table_size = size;
	total_size += size;

	size = sizeof(struct nid_table) + conf.nid_table->size *
			sizeof(struct nid_entry);
	size = (size + pmask) & ~pmask;
	sc.nid_table_offset = total_size;
	sc.nid_table_size = size;
	total_size += size;

	/* Create the file and copy the data there. */
	sprintf(conf.shmem.filename, "/tmp/p4oibd-%d-shm-XXXXXX", getpid());
	conf.shmem.fd = mkstemp(conf.shmem.filename);

	if (conf.shmem.fd == -1)
		return 1;

	ret = ftruncate(conf.shmem.fd, total_size);
	if (ret < 0) {
		printf("ftruncate failed, ret = %d\n", ret);
		close(conf.shmem.fd);
		return 1;
	}

	m = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		conf.shmem.fd, 0);
	if (m == MAP_FAILED)
		return 1;

	conf.shmem.m = (struct shared_config *)m;
	conf.shmem.filesize = total_size;

	/* Copy the header and the tables into the shared memory. */
	memcpy(m, &sc, sizeof(sc));

	memcpy(m + sc.rank_table_offset,
		   conf.rank_table,
		   sc.rank_table_size);
	free(conf.rank_table);
	conf.rank_table = (struct rank_table *)(m + sc.rank_table_offset);

	memcpy(m + sc.nid_table_offset,
		   conf.nid_table, 
		   sc.nid_table_size);
	free(conf.nid_table);
	conf.nid_table = (struct nid_table *)(m + sc.nid_table_offset);

	return 0;
}

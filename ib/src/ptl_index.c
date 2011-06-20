/*
 * ptl_index.c
 */

#include "ptl_loc.h"

/*
 * note you cannot change WORD_SHIFT without changing
 * ffsll and uint64_t to something else
 */

#define SEGMENT_SHIFT	9
#define WORD_SHIFT	6
#define SEGMENT_SIZE	(1 << SEGMENT_SHIFT)
#define WORD_SIZE	(1 << WORD_SHIFT)
#define MAX_INDEX	(SEGMENT_SIZE * SEGMENT_SIZE)
#define MAX_WORD	(MAX_INDEX >> WORD_SHIFT)

static unsigned int next_index;
static uint64_t *index_bit_map;
static void ***index_maps;

static int index_is_init;
static pthread_spinlock_t index_lock;

int index_init(void)
{
	next_index = 0;

	index_bit_map = calloc(SEGMENT_SIZE*SEGMENT_SIZE/8, 1);
	if (!index_bit_map)
		return PTL_NO_SPACE;

	index_maps = calloc(SEGMENT_SIZE, sizeof(void **));
	if (!index_maps) {
		free(index_bit_map);
		return PTL_NO_SPACE;
	}

	pthread_spin_init(&index_lock, PTHREAD_PROCESS_PRIVATE);

	index_is_init = 1;

	return PTL_OK;
}

void index_fini(void)
{
	int i;
	void **p;

	index_is_init = 0;

	pthread_spin_destroy(&index_lock);

	for (i = 0; i < SEGMENT_SIZE; i++) {
		p = index_maps[i];
		if (p)
			free(p);
	}

	free(index_maps);
	free(index_bit_map);
}

int index_get(obj_t *obj, unsigned int *index_p)
{
	unsigned int next_word;
	unsigned int first_word;
	unsigned int last_word;
	unsigned int next_offset;
	unsigned int index;
	uint64_t bits;
	void **map;

	if (!index_is_init) {
		WARN();
		return PTL_FAIL;
	}

	first_word = next_index >> WORD_SHIFT;
	last_word = MAX_WORD;

	next_word = first_word;
	next_offset = next_index & (WORD_SIZE - 1);

	pthread_spin_lock(&index_lock);
	while (next_word < last_word) {
		/* find first clear bit after next_offset */
		bits = ~index_bit_map[next_word];
		bits &= ~((1ULL << next_offset) - 1ULL);

		index = ffsll(bits);
		if (index)
			goto found_bit;

		next_word++;
		next_offset = 0;

		if (next_word == MAX_WORD) {
			next_word = 0;
			next_offset = 0;
			last_word = first_word;
		}
	}

	pthread_spin_unlock(&index_lock);
	WARN();
	return PTL_NO_SPACE;

found_bit:
	index = (next_word << WORD_SHIFT) + index - 1;
	map = index_maps[index >> SEGMENT_SHIFT];
	if (!map) {
		map = calloc(SEGMENT_SIZE, sizeof(void *));
		if (!map) {
			pthread_spin_unlock(&index_lock);
			WARN();
			return PTL_NO_SPACE;
		}

		index_maps[index >> SEGMENT_SHIFT] = map;
	}

	map[index & (SEGMENT_SIZE - 1)] = obj;
	index_bit_map[next_word] |= (1ULL << (index & (WORD_SIZE - 1)));

	next_index = index + 1;

	if (next_index == MAX_INDEX)
		next_index = 0;

	pthread_spin_unlock(&index_lock);
	*index_p = index;
	return PTL_OK;
}

int index_free(unsigned int index)
{
	unsigned int segment;
	unsigned int offset;
	void **map;

	if (!index_is_init) {
		WARN();
		return PTL_FAIL;
	}

	segment = index >> SEGMENT_SHIFT;
	offset = index & (SEGMENT_SIZE - 1);

	if (segment >= SEGMENT_SIZE) {
		WARN();
		return PTL_FAIL;
	}

	pthread_spin_lock(&index_lock);
	map = index_maps[segment];
	if (!map) {
		WARN();
		goto err;
	}

	if (!map[offset]) {
		WARN();
		goto err;
	}

	map[offset] = NULL;

	if (!(index_bit_map[index >> WORD_SHIFT] & (1ULL << (index & (WORD_SIZE - 1))))) {
		WARN();
		goto err;
	}

	index_bit_map[index >> WORD_SHIFT] &=
		~((1ULL << (index & (WORD_SIZE - 1))));

	pthread_spin_unlock(&index_lock);
	return PTL_OK;

err:
	pthread_spin_unlock(&index_lock);
	return PTL_FAIL;
}

int index_lookup(unsigned int index, obj_t **obj_p)
{
	unsigned int segment;
	unsigned int offset;
	void **map;

	segment = index >> SEGMENT_SHIFT;
	offset = index & (SEGMENT_SIZE - 1);

#ifndef NO_ARG_VALIDATION
	if (!index_is_init) {
		WARN();
		return PTL_FAIL;
	}

	if (segment >= SEGMENT_SIZE) {
		WARN();
		return PTL_FAIL;
	}
#endif

	map = index_maps[segment];
	if (!map || !map[offset]) {
		return PTL_FAIL;
	}

	*obj_p = map[offset];
	return PTL_OK;
}

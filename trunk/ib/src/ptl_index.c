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

unsigned int next_index;
uint64_t *index_bit_map;
void ***index_maps;

int index_init()
{
	next_index = 0;

	index_bit_map = calloc(SEGMENT_SIZE*SEGMENT_SIZE/8, 1);
	if (!index_bit_map)
		return PTL_NO_SPACE;

	index_maps = calloc(SEGMENT_SIZE, sizeof(void *));
	if (!index_maps) {
		free(index_bit_map);
		return PTL_NO_SPACE;
	}

	get_maps();
	return PTL_OK;
}

void index_fini()
{
	int i;
	void *p;

	for (i = 0; i < SEGMENT_SIZE; i++) {
		p = index_maps[i];
		if (p)
			free(p);
	}

	free(index_maps);
	free(index_bit_map);
}

int index_get(void *data, unsigned int *index_p)
{
	unsigned int next_word;
	unsigned int first_word;
	unsigned int last_word;
	unsigned int next_offset;
	unsigned int index;
	uint64_t bits;
	void **map;

	first_word = next_index >> WORD_SHIFT;
	last_word = MAX_WORD;

	next_word = first_word;
	next_offset = next_index & (WORD_SIZE - 1);

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

	return PTL_NO_SPACE;

found_bit:
	index = (next_word << WORD_SHIFT) + index - 1;
	map = index_maps[index >> SEGMENT_SHIFT];
	if (!map) {
		map = calloc(SEGMENT_SIZE, sizeof(void *));
		if (!map)
			return PTL_NO_SPACE;
		get_maps();
		index_maps[index >> SEGMENT_SHIFT] = map;
	}

	map[index & (SEGMENT_SIZE - 1)] = data;
	index_bit_map[next_word] |= (1ULL << (index & (WORD_SIZE - 1)));

	next_index = index + 1;

	if (next_index == MAX_INDEX)
		next_index = 0;

	*index_p = index;
	return PTL_OK;
}

int index_free(unsigned int index)
{
	unsigned int segment;
	unsigned int offset;
	void **map;

	segment = index >> SEGMENT_SHIFT;
	offset = index & (SEGMENT_SIZE - 1);

	if (segment >= SEGMENT_SIZE)
		return PTL_FAIL;

	map = index_maps[segment];
	if (!map)
		return PTL_FAIL;

	map[offset] = NULL;

	index_bit_map[index >> WORD_SHIFT] &=
		~((1 << (index & (WORD_SIZE - 1))));

	return PTL_OK;
}

int index_lookup(unsigned int index, void **data)
{
	unsigned int segment;
	unsigned int offset;
	void **map;

	segment = index >> SEGMENT_SHIFT;
	offset = index & (SEGMENT_SIZE - 1);

	if (segment >= SEGMENT_SIZE)
		return PTL_FAIL;

	map = index_maps[segment];
	if (!map || !map[offset])
		return PTL_FAIL;

	*data = map[offset];
	return PTL_OK;
}

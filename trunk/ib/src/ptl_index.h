/*
 * ptl_index.h -- simple index to object mapping service
 */

/*
 * index_init
 *	must be called before any other calls
 *	return PTL_OK or PTL_NO_SPACE
 */
int index_init();

/*
 * index_fini
 *	frees memory
 */
void index_fini();

/*
 * index_get
 *	allocate a new index and store data
 *	returns PTL_OK or PTL_NO_SPACE
 */
int index_get(void *data, unsigned int *index);

/*
 * index_free
 *	free an index
 *	returns PTL_OK or PTL_FAIL if index not present
 */
int index_free(unsigned int index);

/*
 * index_lookup
 *	lookup index and return data
 *	returns PTL_OK or PTL_FAIL if index not present
 */
int index_lookup(unsigned int index, void **data);

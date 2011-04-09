/*
 * ptl_maps.h
 */

#ifndef PTL_MAPS_H
#define PTL_MAPS_H

#ifdef PTL_CHECK_POINTER

int get_maps(void);

int check_range(void *addr, unsigned long length);

static inline void *ptl_malloc(size_t size) {
	void *ptr;

	ptr = malloc(size);
	get_maps();
	return ptr;
}

static inline void *ptl_calloc(size_t nmemb, size_t size) {
	void *ptr;

	ptr = calloc(nmemb, size);
	get_maps();
	return ptr;
}

#define CHECK_POINTER(addr, type) check_range(addr, sizeof(type))

#define CHECK_RANGE(addr, type, count) check_range(addr, count*sizeof(type))

#else

#define get_maps()			(0)

#define ptl_malloc(x)			malloc((x))

#define ptl_calloc(n, x)		calloc((n), (x))

#define CHECK_POINTER(addr, type)	((addr) == NULL)

#define CHECK_RANGE(addr, type, count)	((addr) == NULL)

#endif

#endif /* PTL_MAPS_H */

/**
 * @file ptl_atomic.c
 *
 * Implementation of atomic ops.
 */

#include "ptl_loc.h"

/**
 * Misc useful information about atomic ops
 */
struct atom_op_info op_info[] = {
			/*    float  complex  atomic   swap    uses
			 *	ok	ok	ok	ok   operand	*/
	[PTL_MIN]	= {	1,	0,	1,	0,	0, },
	[PTL_MAX]	= {	1,	0,	1,	0,	0, },
	[PTL_SUM]	= {	1,	1,	1,	0,	0, },
	[PTL_PROD]	= {	1,	1,	1,	0,	0, },
	[PTL_LOR]	= {	0,	0,	1,	0,	0, },
	[PTL_LAND]	= {	0,	0,	1,	0,	0, },
	[PTL_BOR]	= {	0,	0,	1,	0,	0, },
	[PTL_BAND]	= {	0,	0,	1,	0,	0, },
	[PTL_LXOR]	= {	0,	0,	1,	0,	0, },
	[PTL_BXOR]	= {	0,	0,	1,	0,	0, },
	[PTL_SWAP]	= {	1,	1,	0,	1,	0, },
	[PTL_CSWAP]	= {	1,	1,	0,	1,	1, },
	[PTL_MSWAP]	= {	0,	0,	0,	1,	1, },
	[PTL_CSWAP_NE]	= {	1,	1,	0,	1,	1, },
	[PTL_CSWAP_LE]	= {	1,	0,	0,	1,	1, },
	[PTL_CSWAP_LT]	= {	1,	0,	0,	1,	1, },
	[PTL_CSWAP_GE]	= {	1,	0,	0,	1,	1, },
	[PTL_CSWAP_GT]	= {	1,	0,	0,	1,	1, },
};

/**
 * Array of sizes for portals types.
 */
int atom_type_size[] =
{
	[PTL_INT8_T]			= 1,
	[PTL_UINT8_T]			= 1,
	[PTL_INT16_T]			= 2,
	[PTL_UINT16_T]			= 2,
	[PTL_INT32_T]			= 4,
	[PTL_UINT32_T]			= 4,
	[PTL_INT64_T]			= 8,
	[PTL_UINT64_T]			= 8,
	[PTL_FLOAT]			= 4,
	[PTL_FLOAT_COMPLEX]		= 8,
	[PTL_DOUBLE]			= 8,
	[PTL_DOUBLE_COMPLEX]		= 16,

	/* these are system dependant */
	[PTL_LONG_DOUBLE]		= sizeof(long double),
	[PTL_LONG_DOUBLE_COMPLEX]	= 2*sizeof(long double),
};

#define min(a, b)	(((a) < (b)) ? (a) : (b))
#define max(a, b)	(((a) > (b)) ? (a) : (b))
#define sum(a, b)	((a) + (b))
#define prod(a, b)	((a) * (b))
#define lor(a, b)	((a) || (b))
#define land(a, b)	((a) && (b))
#define bor(a, b)	((a) | (b))
#define band(a, b)	((a) & (b))
#define lxor(a, b)	(((a) && !(b)) || (!(a) && (b)))
#define bxor(a, b)	((a) ^ (b))

/**
 * Compute min of two signed char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_sc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_uc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two signed short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_ss(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_us(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two signed int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_si(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_ui(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two signed long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_sl(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_ul(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two float arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_f(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_d(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute min of two long double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int min_ld(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	long double *s = src;
	long double *d = dst;

	for (i = 0; i < length/sizeof(long double); i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two signed char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_sc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_uc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two signed short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_ss(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_us(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two signed int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_si(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_ui(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two signed long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_sl(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_ul(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two float arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_f(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_d(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute max of two long double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int max_ld(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	long double *s = src;
	long double *d = dst;

	for (i = 0; i < length/sizeof(long double); i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two signed char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_sc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_uc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two signed short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_ss(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_us(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two signed int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_si(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_ui(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two signed long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_sl(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_ul(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two float arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_f(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_d(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute sum of two long double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int sum_ld(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	long double *s = src;
	long double *d = dst;

	for (i = 0; i < length/sizeof(long double); i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two signed char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_sc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_uc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two signed short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_ss(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_us(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two signed int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_si(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_ui(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two signed long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_sl(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_ul(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two float arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_f(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two complex float arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_fc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	float *s = src;
	float *d = dst;
	float a, b;

	for (i = 0; i < length/8; i++, s += 2, d += 2) {
		a = prod(s[0], d[0]) - prod(s[1], d[1]);
		b = prod(s[0], d[1]) + prod(s[1], d[0]);
		d[0] = a;
		d[1] = b;
	}

	return PTL_OK;
}

/**
 * Compute prod of two double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_d(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two complex double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_dc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	double *s = src;
	double *d = dst;
	double a, b;

	for (i = 0; i < length/16; i++, s += 2, d += 2) {
		a = prod(s[0], d[0]) - prod(s[1], d[1]);
		b = prod(s[0], d[1]) + prod(s[1], d[0]);
		d[0] = a;
		d[1] = b;
	}

	return PTL_OK;
}

/**
 * Compute prod of two long double arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_ld(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	long double *s = src;
	long double *d = dst;

	for (i = 0; i < length/sizeof(long double); i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

/**
 * Compute prod of two long double complex arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int prod_ldc(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	long double *s = src;
	long double *d = dst;
	long double a, b;

	for (i = 0; i < length/(2*sizeof(long double)); i++, s += 2, d += 2) {
		a = prod(s[0], d[0]) - prod(s[1], d[1]);
		b = prod(s[0], d[1]) + prod(s[1], d[0]);
		d[0] = a;
		d[1] = b;
	}

	return PTL_OK;
}

/**
 * Compute logical or of two signed or unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lor_c(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical or of two signed or unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lor_s(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical or of two signed or unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lor_i(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical or of two signed or unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lor_l(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical and of two signed or unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int land_c(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical and of two signed or unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int land_s(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical and of two signed or unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int land_i(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical and of two signed or unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int land_l(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise or of two signed or unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bor_c(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise or of two signed or unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bor_s(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise or of two signed or unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bor_i(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise or of two signed or unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bor_l(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise and of two signed or unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int band_c(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise and of two signed or unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int band_s(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise and of two signed or unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int band_i(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise and of two signed or unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int band_l(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical xor of two signed or unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lxor_c(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical xor of two signed or unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lxor_s(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical xor of two signed or unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lxor_i(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

/**
 * Compute logical xor of two signed or unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int lxor_l(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise xor of two signed or unsigned char arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bxor_c(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise xor of two signed or unsigned short arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bxor_s(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise xor of two signed or unsigned int arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bxor_i(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

/**
 * Compute bitwise xor of two signed or unsigned long arrays.
 *
 * @param dst destination array
 * @param src source array
 * @param length array length in bytes
 *
 * @return status
 */
static int bxor_l(void *dst, void *src, ptl_size_t length)
{
	ptl_size_t i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

/**
 * An array of function pointers to compute indicated atomic
 * op on two arrays of indicated data type.
 */
atom_op_t atom_op[PTL_OP_LAST][PTL_DATATYPE_LAST] = {
	[PTL_MIN]	= {
		[PTL_INT8_T]		= min_sc,
		[PTL_UINT8_T]		= min_uc,
		[PTL_INT16_T]		= min_ss,
		[PTL_UINT16_T]		= min_us,
		[PTL_INT32_T]		= min_si,
		[PTL_UINT32_T]		= min_ui,
		[PTL_INT64_T]		= min_sl,
		[PTL_UINT64_T]		= min_ul,
		[PTL_FLOAT]		= min_f,
		[PTL_DOUBLE]		= min_d,
		[PTL_LONG_DOUBLE]	= min_ld,
	},
	[PTL_MAX]	= {
		[PTL_INT8_T]		= max_sc,
		[PTL_UINT8_T]		= max_uc,
		[PTL_INT16_T]		= max_ss,
		[PTL_UINT16_T]		= max_us,
		[PTL_INT32_T]		= max_si,
		[PTL_UINT32_T]		= max_ui,
		[PTL_INT64_T]		= max_sl,
		[PTL_UINT64_T]		= max_ul,
		[PTL_FLOAT]		= max_f,
		[PTL_DOUBLE]		= max_d,
		[PTL_LONG_DOUBLE]	= max_ld,
	},
	[PTL_SUM]	= {
		[PTL_INT8_T]		= sum_sc,
		[PTL_UINT8_T]		= sum_uc,
		[PTL_INT16_T]		= sum_ss,
		[PTL_UINT16_T]		= sum_us,
		[PTL_INT32_T]		= sum_si,
		[PTL_UINT32_T]		= sum_ui,
		[PTL_INT64_T]		= sum_sl,
		[PTL_UINT64_T]		= sum_ul,
		[PTL_FLOAT]		= sum_f,
		[PTL_FLOAT_COMPLEX]	= sum_f,
		[PTL_DOUBLE]		= sum_d,
		[PTL_DOUBLE_COMPLEX]	= sum_d,
		[PTL_LONG_DOUBLE]	= sum_ld,
		[PTL_LONG_DOUBLE_COMPLEX] = sum_ld,
	},
	[PTL_PROD]	= {
		[PTL_INT8_T]		= prod_sc,
		[PTL_UINT8_T]		= prod_uc,
		[PTL_INT16_T]		= prod_ss,
		[PTL_UINT16_T]		= prod_us,
		[PTL_INT32_T]		= prod_si,
		[PTL_UINT32_T]		= prod_ui,
		[PTL_INT64_T]		= prod_sl,
		[PTL_UINT64_T]		= prod_ul,
		[PTL_FLOAT]		= prod_f,
		[PTL_FLOAT_COMPLEX]	= prod_fc,
		[PTL_DOUBLE]		= prod_d,
		[PTL_DOUBLE_COMPLEX]	= prod_dc,
		[PTL_LONG_DOUBLE]	= prod_ld,
		[PTL_LONG_DOUBLE_COMPLEX] = prod_ldc,
	},
	[PTL_LOR]	= {
		[PTL_INT8_T]		= lor_c,
		[PTL_UINT8_T]		= lor_c,
		[PTL_INT16_T]		= lor_s,
		[PTL_UINT16_T]		= lor_s,
		[PTL_INT32_T]		= lor_i,
		[PTL_UINT32_T]		= lor_i,
		[PTL_INT64_T]		= lor_l,
		[PTL_UINT64_T]		= lor_l,
	},
	[PTL_LAND]	= {
		[PTL_INT8_T]		= land_c,
		[PTL_UINT8_T]		= land_c,
		[PTL_INT16_T]		= land_s,
		[PTL_UINT16_T]		= land_s,
		[PTL_INT32_T]		= land_i,
		[PTL_UINT32_T]		= land_i,
		[PTL_INT64_T]		= land_l,
		[PTL_UINT64_T]		= land_l,
	},
	[PTL_BOR]	= {
		[PTL_INT8_T]		= bor_c,
		[PTL_UINT8_T]		= bor_c,
		[PTL_INT16_T]		= bor_s,
		[PTL_UINT16_T]		= bor_s,
		[PTL_INT32_T]		= bor_i,
		[PTL_UINT32_T]		= bor_i,
		[PTL_INT64_T]		= bor_l,
		[PTL_UINT64_T]		= bor_l,
	},
	[PTL_BAND]	= {
		[PTL_INT8_T]		= band_c,
		[PTL_UINT8_T]		= band_c,
		[PTL_INT16_T]		= band_s,
		[PTL_UINT16_T]		= band_s,
		[PTL_INT32_T]		= band_i,
		[PTL_UINT32_T]		= band_i,
		[PTL_INT64_T]		= band_l,
		[PTL_UINT64_T]		= band_l,
	},
	[PTL_LXOR]	= {
		[PTL_INT8_T]		= lxor_c,
		[PTL_UINT8_T]		= lxor_c,
		[PTL_INT16_T]		= lxor_s,
		[PTL_UINT16_T]		= lxor_s,
		[PTL_INT32_T]		= lxor_i,
		[PTL_UINT32_T]		= lxor_i,
		[PTL_INT64_T]		= lxor_l,
		[PTL_UINT64_T]		= lxor_l,
	},
	[PTL_BXOR]	= {
		[PTL_INT8_T]		= bxor_c,
		[PTL_UINT8_T]		= bxor_c,
		[PTL_INT16_T]		= bxor_s,
		[PTL_UINT16_T]		= bxor_s,
		[PTL_INT32_T]		= bxor_i,
		[PTL_UINT32_T]		= bxor_i,
		[PTL_INT64_T]		= bxor_l,
		[PTL_UINT64_T]		= bxor_l,
	},
};

#define cswap(op, s, d, type)							\
	do {	if (op->type == d->type) d->type = s->type; } while (0)

#define cswap_c(op, s, d, type)							\
	do {	if (op->type[0] == d->type[0] && op->type[1] == d->type[1]) {	\
		d->type[0] = s->type[0];					\
		d->type[1] = s->type[1];					\
	} } while (0);

#define cswap_ne(op, s, d, type)						\
	do {	if (op->type != d->type) d->type = s->type; } while (0)

#define cswap_ne_c(op, s, d, type)						\
	do {	if (op->type[0] != d->type[0] || op->type[1] != d->type[1]) {	\
		d->type[0] = s->type[0];					\
		d->type[1] = s->type[1];					\
	} } while (0);

#define cswap_le(op, s, d, type)						\
	do {	if (op->type <= d->type) d->type = s->type; } while (0)

#define cswap_lt(op, s, d, type)						\
	do {	if (op->type < d->type) d->type = s->type; } while (0)

#define cswap_ge(op, s, d, type)						\
	do {	if (op->type >= d->type) d->type = s->type; } while (0)

#define cswap_gt(op, s, d, type)						\
	do {	if (op->type > d->type) d->type = s->type; } while (0)

#define mswap(op, s, d, type)							\
	do {	d->type = (op->type & s->type) |				\
		(~op->type & d->type); } while (0)

/**
 * perform swap operations for all cases except PTL_SWAP.
 *
 * @todo There is an open issue with the operand for double complex
 * swap operations. Since operand is declared as u64 and that
 * is not big enough to hold a double complex.
 *
 * @param atom_op The swap operation to perform
 * @param atom_type The data type to use
 * @param dest address of target data
 * @param source address of source data
 * @param operand address of operand
 *
 * @return status
 */
int swap_data_in(ptl_op_t atom_op, ptl_datatype_t atom_type,
		 void *dest, void *source, void *operand)
{
	datatype_t *opr = operand;
	datatype_t *src = source;
	datatype_t *dst = dest;

	switch (atom_op) {
	case PTL_CSWAP:
		switch (atom_type) {
		case PTL_INT8_T:
			cswap(opr, src, dst, s8);
			break;
		case PTL_UINT8_T:
			cswap(opr, src, dst, u8);
			break;
		case PTL_INT16_T:
			cswap(opr, src, dst, s16);
			break;
		case PTL_UINT16_T:
			cswap(opr, src, dst, u16);
			break;
		case PTL_INT32_T:
			cswap(opr, src, dst, s32);
			break;
		case PTL_UINT32_T:
			cswap(opr, src, dst, u32);
			break;
		case PTL_INT64_T:
			cswap(opr, src, dst, s64);
			break;
		case PTL_UINT64_T:
			cswap(opr, src, dst, u64);
			break;
		case PTL_FLOAT:
			cswap(opr, src, dst, f);
			break;
		case PTL_FLOAT_COMPLEX:
			cswap_c(opr, src, dst, fc);
			break;
		case PTL_DOUBLE:
			cswap(opr, src, dst, d);
			break;
		case PTL_DOUBLE_COMPLEX:
			cswap_c(opr, src, dst, dc);
			break;
		case PTL_LONG_DOUBLE:
			cswap(opr, src, dst, ld);
			break;
		case PTL_LONG_DOUBLE_COMPLEX:
			cswap_c(opr, src, dst, ldc);
			break;
		default:
			return PTL_ARG_INVALID;
		}
		break;
	case PTL_CSWAP_NE:
		switch (atom_type) {
		case PTL_INT8_T:
			cswap_ne(opr, src, dst, s8);
			break;
		case PTL_UINT8_T:
			cswap_ne(opr, src, dst, u8);
			break;
		case PTL_INT16_T:
			cswap_ne(opr, src, dst, s16);
			break;
		case PTL_UINT16_T:
			cswap_ne(opr, src, dst, u16);
			break;
		case PTL_INT32_T:
			cswap_ne(opr, src, dst, s32);
			break;
		case PTL_UINT32_T:
			cswap_ne(opr, src, dst, u32);
			break;
		case PTL_INT64_T:
			cswap_ne(opr, src, dst, s64);
			break;
		case PTL_UINT64_T:
			cswap_ne(opr, src, dst, u64);
			break;
		case PTL_FLOAT:
			cswap_ne(opr, src, dst, f);
			break;
		case PTL_FLOAT_COMPLEX:
			cswap_ne_c(opr, src, dst, fc);
			break;
		case PTL_DOUBLE:
			cswap_ne(opr, src, dst, d);
			break;
		case PTL_DOUBLE_COMPLEX:
			cswap_ne_c(opr, src, dst, dc);
			break;
		case PTL_LONG_DOUBLE:
			cswap_ne(opr, src, dst, ld);
			break;
		case PTL_LONG_DOUBLE_COMPLEX:
			cswap_ne_c(opr, src, dst, ldc);
			break;
		default:
			return PTL_ARG_INVALID;
		}
		break;
	case PTL_CSWAP_LE:
		switch (atom_type) {
		case PTL_INT8_T:
			cswap_le(opr, src, dst, s8);
			break;
		case PTL_UINT8_T:
			cswap_le(opr, src, dst, u8);
			break;
		case PTL_INT16_T:
			cswap_le(opr, src, dst, s16);
			break;
		case PTL_UINT16_T:
			cswap_le(opr, src, dst, u16);
			break;
		case PTL_INT32_T:
			cswap_le(opr, src, dst, s32);
			break;
		case PTL_UINT32_T:
			cswap_le(opr, src, dst, u32);
			break;
		case PTL_INT64_T:
			cswap_le(opr, src, dst, s64);
			break;
		case PTL_UINT64_T:
			cswap_le(opr, src, dst, u64);
			break;
		case PTL_FLOAT:
			cswap_le(opr, src, dst, f);
			break;
		case PTL_DOUBLE:
			cswap_le(opr, src, dst, d);
			break;
		case PTL_LONG_DOUBLE:
			cswap_le(opr, src, dst, ld);
			break;
		default:
			return PTL_ARG_INVALID;
		}
		break;
	case PTL_CSWAP_LT:
		switch (atom_type) {
		case PTL_INT8_T:
			cswap_lt(opr, src, dst, s8);
			break;
		case PTL_UINT8_T:
			cswap_lt(opr, src, dst, u8);
			break;
		case PTL_INT16_T:
			cswap_lt(opr, src, dst, s16);
			break;
		case PTL_UINT16_T:
			cswap_lt(opr, src, dst, u16);
			break;
		case PTL_INT32_T:
			cswap_lt(opr, src, dst, s32);
			break;
		case PTL_UINT32_T:
			cswap_lt(opr, src, dst, u32);
			break;
		case PTL_INT64_T:
			cswap_lt(opr, src, dst, s64);
			break;
		case PTL_UINT64_T:
			cswap_lt(opr, src, dst, u64);
			break;
		case PTL_FLOAT:
			cswap_lt(opr, src, dst, f);
			break;
		case PTL_DOUBLE:
			cswap_lt(opr, src, dst, d);
			break;
		case PTL_LONG_DOUBLE:
			cswap_lt(opr, src, dst, ld);
			break;
		default:
			return PTL_ARG_INVALID;
		}
		break;
	case PTL_CSWAP_GE:
		switch (atom_type) {
		case PTL_INT8_T:
			cswap_ge(opr, src, dst, s8);
			break;
		case PTL_UINT8_T:
			cswap_ge(opr, src, dst, u8);
			break;
		case PTL_INT16_T:
			cswap_ge(opr, src, dst, s16);
			break;
		case PTL_UINT16_T:
			cswap_ge(opr, src, dst, u16);
			break;
		case PTL_INT32_T:
			cswap_ge(opr, src, dst, s32);
			break;
		case PTL_UINT32_T:
			cswap_ge(opr, src, dst, u32);
			break;
		case PTL_INT64_T:
			cswap_ge(opr, src, dst, s64);
			break;
		case PTL_UINT64_T:
			cswap_ge(opr, src, dst, u64);
			break;
		case PTL_FLOAT:
			cswap_ge(opr, src, dst, f);
			break;
		case PTL_DOUBLE:
			cswap_ge(opr, src, dst, d);
			break;
		case PTL_LONG_DOUBLE:
			cswap_ge(opr, src, dst, ld);
			break;
		default:
			return PTL_ARG_INVALID;
		}
		break;
	case PTL_CSWAP_GT:
		switch (atom_type) {
		case PTL_INT8_T:
			cswap_gt(opr, src, dst, s8);
			break;
		case PTL_UINT8_T:
			cswap_gt(opr, src, dst, u8);
			break;
		case PTL_INT16_T:
			cswap_gt(opr, src, dst, s16);
			break;
		case PTL_UINT16_T:
			cswap_gt(opr, src, dst, u16);
			break;
		case PTL_INT32_T:
			cswap_gt(opr, src, dst, s32);
			break;
		case PTL_UINT32_T:
			cswap_gt(opr, src, dst, u32);
			break;
		case PTL_INT64_T:
			cswap_gt(opr, src, dst, s64);
			break;
		case PTL_UINT64_T:
			cswap_gt(opr, src, dst, u64);
			break;
		case PTL_FLOAT:
			cswap_gt(opr, src, dst, f);
			break;
		case PTL_DOUBLE:
			cswap_gt(opr, src, dst, d);
			break;
		case PTL_LONG_DOUBLE:
			cswap_gt(opr, src, dst, ld);
			break;
		default:
			return PTL_ARG_INVALID;
		}
		break;
	case PTL_MSWAP:
		switch (atom_type) {
		case PTL_INT8_T:
		case PTL_UINT8_T:
			mswap(opr, src, dst, u8);
			break;
		case PTL_INT16_T:
		case PTL_UINT16_T:
			mswap(opr, src, dst, u16);
			break;
		case PTL_INT32_T:
		case PTL_UINT32_T:
			mswap(opr, src, dst, u32);
			break;
		case PTL_INT64_T:
		case PTL_UINT64_T:
			mswap(opr, src, dst, u64);
			break;
		default:
			return PTL_ARG_INVALID;
		}
		break;
	default:
		return PTL_ARG_INVALID;
	}

	return PTL_OK;
}

#include "ptl_loc.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef sum
#undef sum
#endif
#ifdef prod
#undef prod
#endif
#ifdef lor
#undef lor
#endif
#ifdef land
#undef land
#endif
#ifdef bor
#undef bor
#endif
#ifdef band
#undef band
#endif
#ifdef lxor
#undef lxor
#endif
#ifdef bxor
#undef bxor
#endif

#define min(a,b)	(((a) < (b)) ? (a) : (b))
#define max(a,b)	(((a) > (b)) ? (a) : (b))
#define sum(a,b)	((a) + (b))
#define prod(a,b)	((a) * (b))
#define lor(a,b)	((a) || (b))
#define land(a,b)	((a) && (b))
#define bor(a,b)	((a) | (b))
#define band(a,b)	((a) & (b))
#define lxor(a,b)	(((a) && !(b)) || (!(a) && (b)))
#define bxor(a,b)	((a) ^ (b))

static int min_sc(void *dst, void *src, ptl_size_t length)
{
	int i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_uc(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_ss(void *dst, void *src, ptl_size_t length)
{
	int i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_us(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_si(void *dst, void *src, ptl_size_t length)
{
	int i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_ui(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_sl(void *dst, void *src, ptl_size_t length)
{
	int i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_ul(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_f(void *dst, void *src, ptl_size_t length)
{
	int i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int min_d(void *dst, void *src, ptl_size_t length)
{
	int i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = min(*s, *d);

	return PTL_OK;
}

static int max_sc(void *dst, void *src, ptl_size_t length)
{
	int i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_uc(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_ss(void *dst, void *src, ptl_size_t length)
{
	int i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_us(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_si(void *dst, void *src, ptl_size_t length)
{
	int i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_ui(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_sl(void *dst, void *src, ptl_size_t length)
{
	int i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_ul(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_f(void *dst, void *src, ptl_size_t length)
{
	int i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int max_d(void *dst, void *src, ptl_size_t length)
{
	int i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = max(*s, *d);

	return PTL_OK;
}

static int sum_sc(void *dst, void *src, ptl_size_t length)
{
	int i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_uc(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_ss(void *dst, void *src, ptl_size_t length)
{
	int i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_us(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_si(void *dst, void *src, ptl_size_t length)
{
	int i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_ui(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_sl(void *dst, void *src, ptl_size_t length)
{
	int i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_ul(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_f(void *dst, void *src, ptl_size_t length)
{
	int i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int sum_d(void *dst, void *src, ptl_size_t length)
{
	int i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = sum(*s, *d);

	return PTL_OK;
}

static int prod_sc(void *dst, void *src, ptl_size_t length)
{
	int i;
	int8_t *s = src;
	int8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_uc(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_ss(void *dst, void *src, ptl_size_t length)
{
	int i;
	int16_t *s = src;
	int16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_us(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_si(void *dst, void *src, ptl_size_t length)
{
	int i;
	int32_t *s = src;
	int32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_ui(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_sl(void *dst, void *src, ptl_size_t length)
{
	int i;
	int64_t *s = src;
	int64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_ul(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_f(void *dst, void *src, ptl_size_t length)
{
	int i;
	float *s = src;
	float *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_fc(void *dst, void *src, ptl_size_t length)
{
	int i;
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

static int prod_d(void *dst, void *src, ptl_size_t length)
{
	int i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = prod(*s, *d);

	return PTL_OK;
}

static int prod_dc(void *dst, void *src, ptl_size_t length)
{
	int i;
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

static int lor_c(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

static int lor_s(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

static int lor_i(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

static int lor_l(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = lor(*s, *d);

	return PTL_OK;
}

static int land_c(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

static int land_s(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

static int land_i(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

static int land_l(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = land(*s, *d);

	return PTL_OK;
}

static int bor_c(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

static int bor_s(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

static int bor_i(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

static int bor_l(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = bor(*s, *d);

	return PTL_OK;
}

static int band_c(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

static int band_s(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

static int band_i(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

static int band_l(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = band(*s, *d);

	return PTL_OK;
}

static int lxor_c(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

static int lxor_s(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

static int lxor_i(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

static int lxor_l(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = lxor(*s, *d);

	return PTL_OK;
}

static int bxor_c(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint8_t *s = src;
	uint8_t *d = dst;

	for (i = 0; i < length; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

static int bxor_s(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint16_t *s = src;
	uint16_t *d = dst;

	for (i = 0; i < length/2; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

static int bxor_i(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint32_t *s = src;
	uint32_t *d = dst;

	for (i = 0; i < length/4; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

static int bxor_l(void *dst, void *src, ptl_size_t length)
{
	int i;
	uint64_t *s = src;
	uint64_t *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = bxor(*s, *d);

	return PTL_OK;
}

atom_op_t atom_op[PTL_OP_LAST][PTL_DATATYPE_LAST] = {
	[PTL_MIN]	= {
		[PTL_INT8_T]	= min_sc,
		[PTL_UINT8_T]	= min_uc,
		[PTL_INT16_T]	= min_ss,
		[PTL_UINT16_T]	= min_us,
		[PTL_INT32_T]	= min_si,
		[PTL_UINT32_T]	= min_ui,
		[PTL_INT64_T]	= min_sl,
		[PTL_UINT64_T]	= min_ul,
		[PTL_FLOAT]	= min_f,
		[PTL_DOUBLE]	= min_d,
	},
	[PTL_MAX]	= {
		[PTL_INT8_T]	= max_sc,
		[PTL_UINT8_T]	= max_uc,
		[PTL_INT16_T]	= max_ss,
		[PTL_UINT16_T]	= max_us,
		[PTL_INT32_T]	= max_si,
		[PTL_UINT32_T]	= max_ui,
		[PTL_INT64_T]	= max_sl,
		[PTL_UINT64_T]	= max_ul,
		[PTL_FLOAT]	= max_f,
		[PTL_DOUBLE]	= max_d,
	},
	[PTL_SUM]	= {
		[PTL_INT8_T]	= sum_sc,
		[PTL_UINT8_T]	= sum_uc,
		[PTL_INT16_T]	= sum_ss,
		[PTL_UINT16_T]	= sum_us,
		[PTL_INT32_T]	= sum_si,
		[PTL_UINT32_T]	= sum_ui,
		[PTL_INT64_T]	= sum_sl,
		[PTL_UINT64_T]	= sum_ul,
		[PTL_FLOAT]	= sum_f,
		[PTL_FLOAT_COMPLEX]	= sum_f,
		[PTL_DOUBLE]	= sum_d,
		[PTL_DOUBLE_COMPLEX]	= sum_d,
	},
	[PTL_PROD]	= {
		[PTL_INT8_T]	= prod_sc,
		[PTL_UINT8_T]	= prod_uc,
		[PTL_INT16_T]	= prod_ss,
		[PTL_UINT16_T]	= prod_us,
		[PTL_INT32_T]	= prod_si,
		[PTL_UINT32_T]	= prod_ui,
		[PTL_INT64_T]	= prod_sl,
		[PTL_UINT64_T]	= prod_ul,
		[PTL_FLOAT]	= prod_f,
		[PTL_FLOAT_COMPLEX]	= prod_fc,
		[PTL_DOUBLE]	= prod_d,
		[PTL_DOUBLE_COMPLEX]	= prod_dc,
	},
	[PTL_LOR]	= {
		[PTL_INT8_T]	= lor_c,
		[PTL_UINT8_T]	= lor_c,
		[PTL_INT16_T]	= lor_s,
		[PTL_UINT16_T]	= lor_s,
		[PTL_INT32_T]	= lor_i,
		[PTL_UINT32_T]	= lor_i,
		[PTL_INT64_T]	= lor_l,
		[PTL_UINT64_T]	= lor_l,
	},
	[PTL_LAND]	= {
		[PTL_INT8_T]	= land_c,
		[PTL_UINT8_T]	= land_c,
		[PTL_INT16_T]	= land_s,
		[PTL_UINT16_T]	= land_s,
		[PTL_INT32_T]	= land_i,
		[PTL_UINT32_T]	= land_i,
		[PTL_INT64_T]	= land_l,
		[PTL_UINT64_T]	= land_l,
	},
	[PTL_BOR]	= {
		[PTL_INT8_T]	= bor_c,
		[PTL_UINT8_T]	= bor_c,
		[PTL_INT16_T]	= bor_s,
		[PTL_UINT16_T]	= bor_s,
		[PTL_INT32_T]	= bor_i,
		[PTL_UINT32_T]	= bor_i,
		[PTL_INT64_T]	= bor_l,
		[PTL_UINT64_T]	= bor_l,
	},
	[PTL_BAND]	= {
		[PTL_INT8_T]	= band_c,
		[PTL_UINT8_T]	= band_c,
		[PTL_INT16_T]	= band_s,
		[PTL_UINT16_T]	= band_s,
		[PTL_INT32_T]	= band_i,
		[PTL_UINT32_T]	= band_i,
		[PTL_INT64_T]	= band_l,
		[PTL_UINT64_T]	= band_l,
	},
	[PTL_LXOR]	= {
		[PTL_INT8_T]	= lxor_c,
		[PTL_UINT8_T]	= lxor_c,
		[PTL_INT16_T]	= lxor_s,
		[PTL_UINT16_T]	= lxor_s,
		[PTL_INT32_T]	= lxor_i,
		[PTL_UINT32_T]	= lxor_i,
		[PTL_INT64_T]	= lxor_l,
		[PTL_UINT64_T]	= lxor_l,
	},
	[PTL_BXOR]	= {
		[PTL_INT8_T]	= bxor_c,
		[PTL_UINT8_T]	= bxor_c,
		[PTL_INT16_T]	= bxor_s,
		[PTL_UINT16_T]	= bxor_s,
		[PTL_INT32_T]	= bxor_i,
		[PTL_UINT32_T]	= bxor_i,
		[PTL_INT64_T]	= bxor_l,
		[PTL_UINT64_T]	= bxor_l,
	},
};

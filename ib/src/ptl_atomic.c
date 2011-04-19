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

static int prod_d(void *dst, void *src, ptl_size_t length)
{
	int i;
	double *s = src;
	double *d = dst;

	for (i = 0; i < length/8; i++, s++, d++)
		*d = prod(*s, *d);

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
		[PTL_CHAR]	= min_sc,
		[PTL_UCHAR]	= min_uc,
		[PTL_SHORT]	= min_ss,
		[PTL_USHORT]	= min_us,
		[PTL_INT]	= min_si,
		[PTL_UINT]	= min_ui,
		[PTL_LONG]	= min_sl,
		[PTL_ULONG]	= min_ul,
		[PTL_FLOAT]	= min_f,
		[PTL_DOUBLE]	= min_d,
	},
	[PTL_MAX]	= {
		[PTL_CHAR]	= max_sc,
		[PTL_UCHAR]	= max_uc,
		[PTL_SHORT]	= max_ss,
		[PTL_USHORT]	= max_us,
		[PTL_INT]	= max_si,
		[PTL_UINT]	= max_ui,
		[PTL_LONG]	= max_sl,
		[PTL_ULONG]	= max_ul,
		[PTL_FLOAT]	= max_f,
		[PTL_DOUBLE]	= max_d,
	},
	[PTL_SUM]	= {
		[PTL_CHAR]	= sum_sc,
		[PTL_UCHAR]	= sum_uc,
		[PTL_SHORT]	= sum_ss,
		[PTL_USHORT]	= sum_us,
		[PTL_INT]	= sum_si,
		[PTL_UINT]	= sum_ui,
		[PTL_LONG]	= sum_sl,
		[PTL_ULONG]	= sum_ul,
		[PTL_FLOAT]	= sum_f,
		[PTL_DOUBLE]	= sum_d,
	},
	[PTL_PROD]	= {
		[PTL_CHAR]	= prod_sc,
		[PTL_UCHAR]	= prod_uc,
		[PTL_SHORT]	= prod_ss,
		[PTL_USHORT]	= prod_us,
		[PTL_INT]	= prod_si,
		[PTL_UINT]	= prod_ui,
		[PTL_LONG]	= prod_sl,
		[PTL_ULONG]	= prod_ul,
		[PTL_FLOAT]	= prod_f,
		[PTL_DOUBLE]	= prod_d,
	},
	[PTL_LOR]	= {
		[PTL_CHAR]	= lor_c,
		[PTL_UCHAR]	= lor_c,
		[PTL_SHORT]	= lor_s,
		[PTL_USHORT]	= lor_s,
		[PTL_INT]	= lor_i,
		[PTL_UINT]	= lor_i,
		[PTL_LONG]	= lor_l,
		[PTL_ULONG]	= lor_l,
	},
	[PTL_LAND]	= {
		[PTL_CHAR]	= land_c,
		[PTL_UCHAR]	= land_c,
		[PTL_SHORT]	= land_s,
		[PTL_USHORT]	= land_s,
		[PTL_INT]	= land_i,
		[PTL_UINT]	= land_i,
		[PTL_LONG]	= land_l,
		[PTL_ULONG]	= land_l,
	},
	[PTL_BOR]	= {
		[PTL_CHAR]	= bor_c,
		[PTL_UCHAR]	= bor_c,
		[PTL_SHORT]	= bor_s,
		[PTL_USHORT]	= bor_s,
		[PTL_INT]	= bor_i,
		[PTL_UINT]	= bor_i,
		[PTL_LONG]	= bor_l,
		[PTL_ULONG]	= bor_l,
	},
	[PTL_BAND]	= {
		[PTL_CHAR]	= band_c,
		[PTL_UCHAR]	= band_c,
		[PTL_SHORT]	= band_s,
		[PTL_USHORT]	= band_s,
		[PTL_INT]	= band_i,
		[PTL_UINT]	= band_i,
		[PTL_LONG]	= band_l,
		[PTL_ULONG]	= band_l,
	},
	[PTL_LXOR]	= {
		[PTL_CHAR]	= lxor_c,
		[PTL_UCHAR]	= lxor_c,
		[PTL_SHORT]	= lxor_s,
		[PTL_USHORT]	= lxor_s,
		[PTL_INT]	= lxor_i,
		[PTL_UINT]	= lxor_i,
		[PTL_LONG]	= lxor_l,
		[PTL_ULONG]	= lxor_l,
	},
	[PTL_BXOR]	= {
		[PTL_CHAR]	= bxor_c,
		[PTL_UCHAR]	= bxor_c,
		[PTL_SHORT]	= bxor_s,
		[PTL_USHORT]	= bxor_s,
		[PTL_INT]	= bxor_i,
		[PTL_UINT]	= bxor_i,
		[PTL_LONG]	= bxor_l,
		[PTL_ULONG]	= bxor_l,
	},
};

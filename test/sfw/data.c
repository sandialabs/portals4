/*
 * data.c - datatype utilities
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "../include/portals4.h"
#include "data.h"

char *atom_op_name[] = {
	[PTL_MIN] = "MIN",
	[PTL_MAX] = "MAX",
	[PTL_SUM] = "SUM",
	[PTL_PROD] = "PROD",
	[PTL_LOR] = "LOR",
	[PTL_LAND] = "LAND",
	[PTL_BOR] = "BOR",
	[PTL_BAND] = "BAND",
	[PTL_LXOR] = "LXOR",
	[PTL_BXOR] = "BXOR",
	[PTL_SWAP] = "SWAP",
	[PTL_CSWAP] = "CSWAP",
	[PTL_CSWAP_NE] = "CSWAP_NE",
	[PTL_CSWAP_LE] = "CSWAP_LE",
	[PTL_CSWAP_LT] = "CSWAP_LT",
	[PTL_CSWAP_GE] = "CSWAP_GE",
	[PTL_CSWAP_GT] = "CSWAP_GT",
	[PTL_MSWAP] = "MSWAP",
};

atom_type_t atom_type[] = {
	[PTL_INT8_T] = {.name = "INT8", .size = 1,},
	[PTL_UINT8_T] = {.name = "UINT8", .size = 1,},
	[PTL_INT16_T] = {.name = "INT16", .size = 2,},
	[PTL_UINT16_T] = {.name = "UINT16", .size = 2,},
	[PTL_INT32_T] = {.name = "INT32", .size = 4,},
	[PTL_UINT32_T] = {.name = "UINT32", .size = 4,},
	[PTL_INT64_T] = {.name = "INT64", .size = 8,},
	[PTL_UINT64_T] = {.name = "UINT64", .size = 8,},
	[PTL_FLOAT] = {.name = "FLOAT", .size = 4,},
	[PTL_FLOAT_COMPLEX] = {.name = "COMPLEX", .size = 8,},
	[PTL_DOUBLE] = {.name = "DOUBLE", .size = 8,},
	[PTL_DOUBLE_COMPLEX] = {.name = "DCOMPLEX", .size = 16,},
};

datatype_t get_data(int type)
{
	datatype_t data;

	data.u64 = 0;

	switch (type) {
	case PTL_INT8_T:
	case PTL_UINT8_T:
		data.u8 = random();
		break;
	case PTL_INT16_T:
	case PTL_UINT16_T:
		data.u16 = random();
		break;
	case PTL_INT32_T:
	case PTL_UINT32_T:
		data.u32 = random();
		break;
	case PTL_INT64_T:
	case PTL_UINT64_T:
		data.u64 = random();
		data.u64 = data.u64 << 32 | random();
		break;
	case PTL_FLOAT:
		data.f = random()/65384.0/65384.0;
		break;
	case PTL_FLOAT_COMPLEX:
		data.fc[0] = random()/65384.0/65384.0;
		data.fc[1] = random()/65384.0/65384.0;
		break;
	case PTL_DOUBLE:
		data.d = random()/65384.0/65384.0;
		break;
	case PTL_DOUBLE_COMPLEX:
		data.dc[0] = random()/65384.0/65384.0;
		data.dc[1] = random()/65384.0/65384.0;
		break;
	}

	return data;
}

char *datatype_str(int type, datatype_t data)
{
	static char str[64];

	switch(type) {
	case PTL_INT8_T:
	case PTL_UINT8_T:
		sprintf(str, "0x%02x", data.u8);
		break;
	case PTL_INT16_T:
	case PTL_UINT16_T:
		sprintf(str, "0x%04x", data.u16);
		break;
	case PTL_INT32_T:
	case PTL_UINT32_T:
		sprintf(str, "0x%08x", data.u32);
		break;
	case PTL_INT64_T:
		sprintf(str, "%" PRId64 , data.s64);
		break;
	case PTL_UINT64_T:
		sprintf(str, "0x%016" PRIx64 , data.u64);
		break;
	case PTL_FLOAT:
		sprintf(str, "%12.10f", data.f);
		break;
	case PTL_FLOAT_COMPLEX:
		sprintf(str, "(%12.10f, %12.10f)", data.fc[0], data.fc[1]);
		break;
	case PTL_DOUBLE:
		sprintf(str, "%22.20lf", data.d);
		break;
	case PTL_DOUBLE_COMPLEX:
		sprintf(str, "(%22.20f, %22.20f)", data.dc[0], data.dc[1]);
		break;
	}

	return str;
}

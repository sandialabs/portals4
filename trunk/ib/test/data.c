/*
 * data.c - datatype utilities
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
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
	[PTL_CHAR] = {.name = "CHAR", .size = 1,},
	[PTL_UCHAR] = {.name = "UCHAR", .size = 1,},
	[PTL_SHORT] = {.name = "SHORT", .size = 2,},
	[PTL_USHORT] = {.name = "USHORT", .size = 2,},
	[PTL_INT] = {.name = "INT", .size = 4,},
	[PTL_UINT] = {.name = "UINT", .size = 4,},
	[PTL_LONG] = {.name = "LONG", .size = 8,},
	[PTL_ULONG] = {.name = "ULONG", .size = 8,},
	[PTL_FLOAT] = {.name = "FLOAT", .size = 4,},
	[PTL_DOUBLE] = {.name = "DOUBLE", .size = 8,},
};

datatype_t get_data(int type)
{
	datatype_t data;

	data.u64 = 0;

	switch (type) {
	case PTL_CHAR:
	case PTL_UCHAR:
		data.u8 = random();
		break;
	case PTL_SHORT:
	case PTL_USHORT:
		data.u16 = random();
		break;
	case PTL_INT:
	case PTL_UINT:
		data.u32 = random();
		break;
	case PTL_LONG:
	case PTL_ULONG:
		data.u64 = random();
		data.u64 = data.u64 << 32 | random();
		break;
	case PTL_FLOAT:
		data.f = random()/65384.0/65384.0;
		break;
	case PTL_DOUBLE:
		data.d = random()/65384.0/65384.0;
		break;
	}

	return data;
}

char *datatype_str(int type, datatype_t data)
{
	static char str[64];

	switch(type) {
	case PTL_CHAR:
	case PTL_UCHAR:
		sprintf(str, "0x%02x", data.u8);
		break;
	case PTL_SHORT:
	case PTL_USHORT:
		sprintf(str, "0x%04x", data.u16);
		break;
	case PTL_INT:
	case PTL_UINT:
		sprintf(str, "0x%08x", data.u32);
		break;
	case PTL_LONG:
		sprintf(str, "%ld", data.s64);
		break;
	case PTL_ULONG:
		sprintf(str, "0x%016lx", data.u64);
		break;
	case PTL_FLOAT:
		sprintf(str, "%12.10f", data.f);
		break;
	case PTL_DOUBLE:
		sprintf(str, "%22.20lf", data.d);
		break;
	}

	return str;
}

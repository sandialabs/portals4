#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <inttypes.h>
#include "../include/portals4.h"
#include "data.h"

static unsigned int seed;
static unsigned int physical = 0;
static unsigned int count = 1;
static unsigned int max_length = 32;
static int fetch;

static datatype_t get_result(datatype_t x, datatype_t y, int op, int type)
{
	datatype_t z;

	z.u64 = 0;

	switch(op) {
	case PTL_MIN:
		switch (type) {
		case PTL_CHAR:
			z.s8 = ((x.s8 < y.s8) ? x.s8 : y.s8);
			break;
		case PTL_UCHAR:
			z.u8 = ((x.u8 < y.u8) ? x.u8 : y.u8);
			break;
		case PTL_SHORT:
			z.s16 = ((x.s16 < y.s16) ? x.s16 : y.s16);
			break;
		case PTL_USHORT:
			z.u16 = ((x.u16 < y.u16) ? x.u16 : y.u16);
			break;
		case PTL_INT:
			z.s32 = ((x.s32 < y.s32) ? x.s32 : y.s32);
			break;
		case PTL_UINT:
			z.u32 = ((x.u32 < y.u32) ? x.u32 : y.u32);
			break;
		case PTL_LONG:
			z.s64 = ((x.s64 < y.s64) ? x.s64 : y.s64);
			break;
		case PTL_ULONG:
			z.u64 = ((x.u64 < y.u64) ? x.u64 : y.u64);
			break;
		case PTL_FLOAT:
			z.f = ((x.f < y.f) ? x.f : y.f);
			break;
		case PTL_DOUBLE:
			z.d = ((x.d < y.d) ? x.d : y.d);
			break;
		}
		break;
	case PTL_MAX:
		switch (type) {
		case PTL_CHAR:
			z.s8 = ((x.s8 > y.s8) ? x.s8 : y.s8);
			break;
		case PTL_UCHAR:
			z.u8 = ((x.u8 > y.u8) ? x.u8 : y.u8);
			break;
		case PTL_SHORT:
			z.s16 = ((x.s16 > y.s16) ? x.s16 : y.s16);
			break;
		case PTL_USHORT:
			z.u16 = ((x.u16 > y.u16) ? x.u16 : y.u16);
			break;
		case PTL_INT:
			z.s32 = ((x.s32 > y.s32) ? x.s32 : y.s32);
			break;
		case PTL_UINT:
			z.u32 = ((x.u32 > y.u32) ? x.u32 : y.u32);
			break;
		case PTL_LONG:
			z.s64 = ((x.s64 > y.s64) ? x.s64 : y.s64);
			break;
		case PTL_ULONG:
			z.u64 = ((x.u64 > y.u64) ? x.u64 : y.u64);
			break;
		case PTL_FLOAT:
			z.f = ((x.f > y.f) ? x.f : y.f);
			break;
		case PTL_DOUBLE:
			z.d = ((x.d > y.d) ? x.d : y.d);
			break;
		}
		break;
	case PTL_SUM:
		switch (type) {
		case PTL_CHAR:
			z.s8 = x.s8 + y.s8;
			break;
		case PTL_UCHAR:
			z.u8 = x.u8 + y.u8;
			break;
		case PTL_SHORT:
			z.s16 = x.s16 + y.s16;
			break;
		case PTL_USHORT:
			z.u16 = x.u16 + y.u16;
			break;
		case PTL_INT:
			z.s32 = x.s32 + y.s32;
			break;
		case PTL_UINT:
			z.u32 = x.u32 + y.u32;
			break;
		case PTL_LONG:
			z.s64 = x.s64 + y.s64;
			break;
		case PTL_ULONG:
			z.u64 = x.u64 + y.u64;
			break;
		case PTL_FLOAT:
			z.f = x.f + y.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc[0] = x.fc[0] + y.fc[0];
			z.fc[1] = x.fc[1] + y.fc[1];
			break;
		case PTL_DOUBLE:
			z.d = x.d + y.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc[0] = x.dc[0] + y.dc[0];
			z.dc[1] = x.dc[1] + y.dc[1];
			break;
		}
		break;
	case PTL_PROD:
		switch (type) {
		case PTL_CHAR:
			z.s8 = x.s8 * y.s8;
			break;
		case PTL_UCHAR:
			z.u8 = x.u8 * y.u8;
			break;
		case PTL_SHORT:
			z.s16 = x.s16 * y.s16;
			break;
		case PTL_USHORT:
			z.u16 = x.u16 * y.u16;
			break;
		case PTL_INT:
			z.s32 = x.s32 * y.s32;
			break;
		case PTL_UINT:
			z.u32 = x.u32 * y.u32;
			break;
		case PTL_LONG:
			z.s64 = x.s64 * y.s64;
			break;
		case PTL_ULONG:
			z.u64 = x.u64 * y.u64;
			break;
		case PTL_FLOAT:
			z.f = x.f * y.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc[0] = (x.fc[0]*y.fc[0]) - (x.fc[1]*y.fc[1]);
			z.fc[1] = (x.fc[0]*y.fc[1]) + (x.fc[1]*y.fc[0]);
			break;
		case PTL_DOUBLE:
			z.d = x.d * y.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc[0] = (x.dc[0]*y.dc[0]) - (x.dc[1]*y.dc[1]);
			z.dc[1] = (x.dc[0]*y.dc[1]) + (x.dc[1]*y.dc[0]);
			break;
		}
		break;
	case PTL_LOR:
		switch (type) {
		case PTL_CHAR:
			z.s8 = x.s8 || y.s8;
			break;
		case PTL_UCHAR:
			z.u8 = x.u8 || y.u8;
			break;
		case PTL_SHORT:
			z.s16 = x.s16 || y.s16;
			break;
		case PTL_USHORT:
			z.u16 = x.u16 || y.u16;
			break;
		case PTL_INT:
			z.s32 = x.s32 || y.s32;
			break;
		case PTL_UINT:
			z.u32 = x.u32 || y.u32;
			break;
		case PTL_LONG:
			z.s64 = x.s64 || y.s64;
			break;
		case PTL_ULONG:
			z.u64 = x.u64 || y.u64;
			break;
		}
		break;
	case PTL_LAND:
		switch (type) {
		case PTL_CHAR:
			z.s8 = x.s8 && y.s8;
			break;
		case PTL_UCHAR:
			z.u8 = x.u8 && y.u8;
			break;
		case PTL_SHORT:
			z.s16 = x.s16 && y.s16;
			break;
		case PTL_USHORT:
			z.u16 = x.u16 && y.u16;
			break;
		case PTL_INT:
			z.s32 = x.s32 && y.s32;
			break;
		case PTL_UINT:
			z.u32 = x.u32 && y.u32;
			break;
		case PTL_LONG:
			z.s64 = x.s64 && y.s64;
			break;
		case PTL_ULONG:
			z.u64 = x.u64 && y.u64;
			break;
		}
		break;
	case PTL_BOR:
		switch (type) {
		case PTL_CHAR:
			z.s8 = x.s8 | y.s8;
			break;
		case PTL_UCHAR:
			z.u8 = x.u8 | y.u8;
			break;
		case PTL_SHORT:
			z.s16 = x.s16 | y.s16;
			break;
		case PTL_USHORT:
			z.u16 = x.u16 | y.u16;
			break;
		case PTL_INT:
			z.s32 = x.s32 | y.s32;
			break;
		case PTL_UINT:
			z.u32 = x.u32 | y.u32;
			break;
		case PTL_LONG:
			z.s64 = x.s64 | y.s64;
			break;
		case PTL_ULONG:
			z.u64 = x.u64 | y.u64;
			break;
		}
		break;
	case PTL_BAND:
		switch (type) {
		case PTL_CHAR:
			z.s8 = x.s8 & y.s8;
			break;
		case PTL_UCHAR:
			z.u8 = x.u8 & y.u8;
			break;
		case PTL_SHORT:
			z.s16 = x.s16 & y.s16;
			break;
		case PTL_USHORT:
			z.u16 = x.u16 & y.u16;
			break;
		case PTL_INT:
			z.s32 = x.s32 & y.s32;
			break;
		case PTL_UINT:
			z.u32 = x.u32 & y.u32;
			break;
		case PTL_LONG:
			z.s64 = x.s64 & y.s64;
			break;
		case PTL_ULONG:
			z.u64 = x.u64 & y.u64;
			break;
		}
		break;
	case PTL_LXOR:
		switch (type) {
		case PTL_CHAR:
			z.s8 = (x.s8 && !y.s8) || (!x.s8 && y.s8);
			break;
		case PTL_UCHAR:
			z.u8 = (x.u8 && !y.u8) || (!x.u8 && y.u8);
			break;
		case PTL_SHORT:
			z.s16 = (x.s16 && !y.s16) || (!x.s16 && y.s16);
			break;
		case PTL_USHORT:
			z.u16 = (x.u16 && !y.u16) || (!x.u16 && y.u16);
			break;
		case PTL_INT:
			z.s32 = (x.s32 && !y.s32) || (!x.s32 && y.s32);
			break;
		case PTL_UINT:
			z.u32 = (x.u32 && !y.u32) || (!x.u32 && y.u32);
			break;
		case PTL_LONG:
			z.s64 = (x.s64 && !y.s64) || (!x.s64 && y.s64);
			break;
		case PTL_ULONG:
			z.u64 = (x.u64 && !y.u64) || (!x.u64 && y.u64);
			break;
		}
		break;
	case PTL_BXOR:
		switch (type) {
		case PTL_CHAR:
			z.s8 = x.s8 ^ y.s8;
			break;
		case PTL_UCHAR:
			z.u8 = x.u8 ^ y.u8;
			break;
		case PTL_SHORT:
			z.s16 = x.s16 ^ y.s16;
			break;
		case PTL_USHORT:
			z.u16 = x.u16 ^ y.u16;
			break;
		case PTL_INT:
			z.s32 = x.s32 ^ y.s32;
			break;
		case PTL_UINT:
			z.u32 = x.u32 ^ y.u32;
			break;
		case PTL_LONG:
			z.s64 = x.s64 ^ y.s64;
			break;
		case PTL_ULONG:
			z.u64 = x.u64 ^ y.u64;
			break;
		}
		break;
	}

	return z;
}

static void usage(void)
{
	printf("usage:\n");
	printf("\n");
	printf("make_test_atomic [OPTIONS] > output_file\n");
	printf("\n");
	printf("SYNOPSYS:\n");
	printf("Generate random portals4 test cases for the PtlAtomic operation to standard out.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("	-h | --help			 print this message\n");
	printf("	-f | --fetch			 generate fetch tests\n");
	printf("	-s | --seed		seed	 set random number seed (default time())\n");
	printf("	-p | --physical		physical use physical NI (logical NI)\n");
	printf("	-c | --count		count	  set number of test cases (default 1)\n");
	printf("	-m | --max_length	length	  set max message length (>= 8) (default 32)\n");
	printf("\n");
}

static int arg_process(int argc, char *argv[])
{
	int c;
	int option_index = 0;
	static char *opt_string = "hfps:c:m:";
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"fetch", 0, NULL, 'f'},
		{"seed", 1, NULL, 's'},
		{"physical", 0, NULL, 'p'},
		{"count", 1, NULL, 'c'},
		{"max_length", 1, NULL, 'm'},
		{NULL, 0, NULL, 0}
	};

	while (1) {
		c = getopt_long(argc, argv, opt_string,
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			usage();
			exit(0);

		case 'f':
			fetch++;
			break;

		case 's':
			seed = strtol(optarg, NULL, 0);
			break;

		case 'p':
			physical = 1;
			break;

		case 'c':
			count = strtol(optarg, NULL, 0);
			break;

		case 'm':
			max_length = strtol(optarg, NULL, 0);
			if (max_length < 8) {
				printf("max_length too small\n");
				return 1;
			}
			break;

		default:
			return 1;
		}
	}

	if (optind < argc) {
		printf("unexpected argument %s\n", argv[optind]);
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int err;
	int i;
	int match;
	uint64_t match_bits;
	int op;
	int type;
	datatype_t tgt, din, dout, z;
	int length;
	time_t cur_time = time(NULL);
	char *ptl_op;
	char *me_opt;

	seed = cur_time;

	err = arg_process(argc, argv);
	if (err)
		goto done;

	srandom(seed);

	printf("<?xml version=\"1.0\"?>\n");

	printf("<!--\n");
	printf("	file generated %s", ctime(&cur_time));
	printf("		command =");
	for (i = 0; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");
	printf("		seed = %d\n", seed);
	printf("		count = %d\n", count);
	printf("		max_length = %d\n", max_length);
	printf("-->\n");

	printf("<test>\n");

	for (i = 0; i < count; i++) {
		match = random() & 1;
		match_bits = random();
		match_bits = match_bits << 32 | random();

		/* legal combinations */
		do {
			op = random() % (PTL_BXOR + 1);
			type = random() % PTL_DATATYPE_LAST;
		} while((op >= PTL_LOR && type >= PTL_FLOAT) ||
			(op <= PTL_MAX && (type == PTL_FLOAT_COMPLEX ||
					   type == PTL_DOUBLE_COMPLEX)));

		tgt = get_data(type);
		dout = get_data(type);
		din = get_data(type);
		z = get_result(tgt, dout, op, type);

		length = random() % (max_length/atom_type[type].size);
		length = (length + 1)*atom_type[type].size;

		ptl_op = fetch ? "fetch" : "atomic";
		me_opt = fetch ? "OP_GET OP_PUT" : "OP_PUT";

		printf("  <subtest>\n");
		printf("    <desc>Test %s %s/%s length=%d</desc>\n",
			ptl_op, atom_op_name[op], atom_type[type].name, length);
		printf("    <ptl>\n");

		printf("      <ptl_ni ni_opt=\"%s %s\">\n",
	                        match ? "MATCH" : "NO_MATCH",
				physical ? "PHYSICAL" : "LOGICAL");
		printf("        <ptl_pt>\n");

		/* setup me/le */
		if (match)
			printf("          <ptl_me me_opt=\"%s\" me_match=\"0x%" PRIu64 "\" type=\"%s\" me_data=\"%s\">\n",
				me_opt, match_bits, atom_type[type].name, datatype_str(type, tgt));
		else
			printf("          <ptl_le le_opt=\"%s\" type=\"%s\" le_data=\"%s\">\n",
				me_opt, atom_type[type].name, datatype_str(type, tgt));

		/* setup md(s) */
		printf("            <ptl_md type=\"%s\" md_data=\"%s\">\n",
			atom_type[type].name, datatype_str(type, dout));
		if (fetch)
			printf("              <ptl_md type=\"%s\" md_data=\"%s\">\n",
				atom_type[type].name, datatype_str(type, din));

		if (match)
			printf("              <ptl_%s atom_op=\"%s\" atom_type=\"%s\" length=\"%d\" match=\"0x%" PRIu64 "\" target_id=\"SELF\"/>\n",
				ptl_op, atom_op_name[op], atom_type[type].name, length, match_bits);
		else
			printf("              <ptl_%s atom_op=\"%s\" atom_type=\"%s\" length=\"%d\" target_id=\"SELF\"/>\n",
				ptl_op, atom_op_name[op], atom_type[type].name, length);

		/* TODO replace with an event */
		printf("              <msleep count=\"10\"/>\n");

		if (fetch) {
			/* check to see that din data has changed */
			printf("              <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
				length, atom_type[type].name, datatype_str(type, tgt));
			printf("              <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
				atom_type[type].size, atom_type[type].name, length, datatype_str(type, din));

			printf("              </ptl_md>\n");
		}

		/* check to see that dout data has not changed */
	        printf("              <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
	                length, atom_type[type].name, datatype_str(type, dout));
	        printf("              <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
	                atom_type[type].size, atom_type[type].name, length, datatype_str(type, dout));

		printf("            </ptl_md>\n");

		/* check to see that target data has changed */
		if (match) {
			printf("            <check length=\"%d\" type=\"%s\" me_data=\"%s\"/>\n",
				length, atom_type[type].name, datatype_str(type, z));
			printf("            <check length=\"%d\" type=\"%s\" offset=\"%d\" me_data=\"%s\"/>\n",
				atom_type[type].size, atom_type[type].name, length, datatype_str(type, tgt));
		} else {
			printf("            <check length=\"%d\" type=\"%s\" le_data=\"%s\"/>\n",
				length, atom_type[type].name, datatype_str(type, z));
			printf("            <check length=\"%d\" type=\"%s\" offset=\"%d\" le_data=\"%s\"/>\n",
				atom_type[type].size, atom_type[type].name, length, datatype_str(type, tgt));
		}

		if (match)
			printf("          </ptl_me>\n");
		else
			printf("          </ptl_le>\n");

		printf("        </ptl_pt>\n");
		printf("      </ptl_ni>\n");
		printf("    </ptl>\n");
		printf("  </subtest>\n");
	}
	printf("</test>\n");

	err = 0;

done:
	return err;
}

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <inttypes.h>
#include "../include/portals4.h"
#include "data.h"

static unsigned int seed;
static unsigned int max_length = 32;
static unsigned int to_files = 0;
static int casenum = 0;

static datatype_t get_result(datatype_t x, datatype_t y, int op, int type)
{
	datatype_t z;

	z.u64 = 0;

	switch(op) {
	case PTL_MIN:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = ((x.s8 < y.s8) ? x.s8 : y.s8);
			break;
		case PTL_UINT8_T:
			z.u8 = ((x.u8 < y.u8) ? x.u8 : y.u8);
			break;
		case PTL_INT16_T:
			z.s16 = ((x.s16 < y.s16) ? x.s16 : y.s16);
			break;
		case PTL_UINT16_T:
			z.u16 = ((x.u16 < y.u16) ? x.u16 : y.u16);
			break;
		case PTL_INT32_T:
			z.s32 = ((x.s32 < y.s32) ? x.s32 : y.s32);
			break;
		case PTL_UINT32_T:
			z.u32 = ((x.u32 < y.u32) ? x.u32 : y.u32);
			break;
		case PTL_INT64_T:
			z.s64 = ((x.s64 < y.s64) ? x.s64 : y.s64);
			break;
		case PTL_UINT64_T:
			z.u64 = ((x.u64 < y.u64) ? x.u64 : y.u64);
			break;
		case PTL_FLOAT:
			z.f = ((x.f < y.f) ? x.f : y.f);
			break;
		case PTL_DOUBLE:
			z.d = ((x.d < y.d) ? x.d : y.d);
			break;
		case PTL_LONG_DOUBLE:
			z.ld = ((x.ld < y.ld) ? x.ld : y.ld);
			break;
		}
		break;
	case PTL_MAX:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = ((x.s8 > y.s8) ? x.s8 : y.s8);
			break;
		case PTL_UINT8_T:
			z.u8 = ((x.u8 > y.u8) ? x.u8 : y.u8);
			break;
		case PTL_INT16_T:
			z.s16 = ((x.s16 > y.s16) ? x.s16 : y.s16);
			break;
		case PTL_UINT16_T:
			z.u16 = ((x.u16 > y.u16) ? x.u16 : y.u16);
			break;
		case PTL_INT32_T:
			z.s32 = ((x.s32 > y.s32) ? x.s32 : y.s32);
			break;
		case PTL_UINT32_T:
			z.u32 = ((x.u32 > y.u32) ? x.u32 : y.u32);
			break;
		case PTL_INT64_T:
			z.s64 = ((x.s64 > y.s64) ? x.s64 : y.s64);
			break;
		case PTL_UINT64_T:
			z.u64 = ((x.u64 > y.u64) ? x.u64 : y.u64);
			break;
		case PTL_FLOAT:
			z.f = ((x.f > y.f) ? x.f : y.f);
			break;
		case PTL_DOUBLE:
			z.d = ((x.d > y.d) ? x.d : y.d);
			break;
		case PTL_LONG_DOUBLE:
			z.ld = ((x.ld > y.ld) ? x.ld : y.ld);
			break;
		}
		break;
	case PTL_SUM:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = x.s8 + y.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = x.u8 + y.u8;
			break;
		case PTL_INT16_T:
			z.s16 = x.s16 + y.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = x.u16 + y.u16;
			break;
		case PTL_INT32_T:
			z.s32 = x.s32 + y.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = x.u32 + y.u32;
			break;
		case PTL_INT64_T:
			z.s64 = x.s64 + y.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = x.u64 + y.u64;
			break;
		case PTL_FLOAT:
			z.f = x.f + y.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc = x.fc + y.fc;
			break;
		case PTL_DOUBLE:
			z.d = x.d + y.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc = x.dc + y.dc;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = x.ld + y.ld;
			break;
		case PTL_LONG_DOUBLE_COMPLEX:
			z.ldc = x.ldc + y.ldc;
			break;
		}
		break;
	case PTL_PROD:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = x.s8 * y.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = x.u8 * y.u8;
			break;
		case PTL_INT16_T:
			z.s16 = x.s16 * y.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = x.u16 * y.u16;
			break;
		case PTL_INT32_T:
			z.s32 = x.s32 * y.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = x.u32 * y.u32;
			break;
		case PTL_INT64_T:
			z.s64 = x.s64 * y.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = x.u64 * y.u64;
			break;
		case PTL_FLOAT:
			z.f = x.f * y.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc = x.fc * y.fc;
			break;
		case PTL_DOUBLE:
			z.d = x.d * y.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc = x.dc * y.dc;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = x.ld * y.ld;
			break;
		case PTL_LONG_DOUBLE_COMPLEX:
			z.ldc = x.ldc * y.ldc;
			break;
		}
		break;
	case PTL_LOR:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = x.s8 || y.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = x.u8 || y.u8;
			break;
		case PTL_INT16_T:
			z.s16 = x.s16 || y.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = x.u16 || y.u16;
			break;
		case PTL_INT32_T:
			z.s32 = x.s32 || y.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = x.u32 || y.u32;
			break;
		case PTL_INT64_T:
			z.s64 = x.s64 || y.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = x.u64 || y.u64;
			break;
		}
		break;
	case PTL_LAND:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = x.s8 && y.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = x.u8 && y.u8;
			break;
		case PTL_INT16_T:
			z.s16 = x.s16 && y.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = x.u16 && y.u16;
			break;
		case PTL_INT32_T:
			z.s32 = x.s32 && y.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = x.u32 && y.u32;
			break;
		case PTL_INT64_T:
			z.s64 = x.s64 && y.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = x.u64 && y.u64;
			break;
		}
		break;
	case PTL_BOR:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = x.s8 | y.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = x.u8 | y.u8;
			break;
		case PTL_INT16_T:
			z.s16 = x.s16 | y.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = x.u16 | y.u16;
			break;
		case PTL_INT32_T:
			z.s32 = x.s32 | y.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = x.u32 | y.u32;
			break;
		case PTL_INT64_T:
			z.s64 = x.s64 | y.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = x.u64 | y.u64;
			break;
		}
		break;
	case PTL_BAND:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = x.s8 & y.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = x.u8 & y.u8;
			break;
		case PTL_INT16_T:
			z.s16 = x.s16 & y.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = x.u16 & y.u16;
			break;
		case PTL_INT32_T:
			z.s32 = x.s32 & y.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = x.u32 & y.u32;
			break;
		case PTL_INT64_T:
			z.s64 = x.s64 & y.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = x.u64 & y.u64;
			break;
		}
		break;
	case PTL_LXOR:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = (x.s8 && !y.s8) || (!x.s8 && y.s8);
			break;
		case PTL_UINT8_T:
			z.u8 = (x.u8 && !y.u8) || (!x.u8 && y.u8);
			break;
		case PTL_INT16_T:
			z.s16 = (x.s16 && !y.s16) || (!x.s16 && y.s16);
			break;
		case PTL_UINT16_T:
			z.u16 = (x.u16 && !y.u16) || (!x.u16 && y.u16);
			break;
		case PTL_INT32_T:
			z.s32 = (x.s32 && !y.s32) || (!x.s32 && y.s32);
			break;
		case PTL_UINT32_T:
			z.u32 = (x.u32 && !y.u32) || (!x.u32 && y.u32);
			break;
		case PTL_INT64_T:
			z.s64 = (x.s64 && !y.s64) || (!x.s64 && y.s64);
			break;
		case PTL_UINT64_T:
			z.u64 = (x.u64 && !y.u64) || (!x.u64 && y.u64);
			break;
		}
		break;
	case PTL_BXOR:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = x.s8 ^ y.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = x.u8 ^ y.u8;
			break;
		case PTL_INT16_T:
			z.s16 = x.s16 ^ y.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = x.u16 ^ y.u16;
			break;
		case PTL_INT32_T:
			z.s32 = x.s32 ^ y.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = x.u32 ^ y.u32;
			break;
		case PTL_INT64_T:
			z.s64 = x.s64 ^ y.s64;
			break;
		case PTL_UINT64_T:
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
	printf("Generate portals4 coverage test cases for the PtlAtomic operation to standard out.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("    -h | --help                   print this message\n");
	printf("    -s | --seed <seed>            set random number seed (default time())\n");
	printf("    -m | --max_length <length>    set max message length (>= 8) (default 32)\n");
	printf("    -w | --write                  output to separate files\n");
	printf("\n");
}

static int arg_process(int argc, char *argv[])
{
	int c;
	int option_index = 0;
	static char *opt_string = "hs:m:w";
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"seed", 1, NULL, 's'},
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

		case 's':
			seed = strtol(optarg, NULL, 0);
			break;

		case 'w':
			to_files = 1;
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

static void generate_case(int op, int type, int match, uint64_t match_bits,
						  datatype_t din,
						  datatype_t dout,
						  datatype_t tgt,
						  datatype_t z,
						  int length,
						  int fetch)
{
	FILE *f;
	char *ptl_op;
	char *me_opt;

	if (to_files) {
		char fname[1000];
		
		casenum ++;		
		sprintf(fname, "temp/test_atomic_all-%03d.xml", casenum);
		f = fopen(fname, "w");
		if (f == NULL) {
			fprintf(stderr, "Cannot create file %s - aborting!\n", fname);
			exit(1);
		}
	} else {
		f = stdout;
	}

	fprintf(f, "<?xml version=\"1.0\"?>\n");

	fprintf(f, "<!--\n");
	fprintf(f, "		seed = %d\n", seed);
	fprintf(f, "		max_length = %d\n", max_length);
	fprintf(f, "-->\n");

	fprintf(f, "<test>\n");

	ptl_op = fetch ? "fetch" : "atomic";
	me_opt = fetch ? "OP_GET OP_PUT" : "OP_PUT";

	fprintf(f, "  <desc>Test %s %s/%s length=%d</desc>\n",
		   ptl_op, atom_op_name[op], atom_type[type].name, length);
	fprintf(f, "  <ptl>\n");

	fprintf(f, "    <ptl_ni ni_opt=\"%s %s\">\n",
		   match ? "MATCH" : "NO_MATCH",
		   "PHYSICAL");
	fprintf(f, "      <ptl_pt>\n");

	/* setup me/le */
	if (match)
		fprintf(f, "        <ptl_me me_opt=\"%s\" me_match=\"0x%" PRIu64 "\" type=\"%s\" me_data=\"%s\">\n",
			   me_opt, match_bits, atom_type[type].name, datatype_str(type, tgt));
	else
		fprintf(f, "        <ptl_le le_opt=\"%s\" type=\"%s\" le_data=\"%s\">\n",
			   me_opt, atom_type[type].name, datatype_str(type, tgt));

	/* setup md(s) */
	fprintf(f, "          <ptl_md type=\"%s\" md_data=\"%s\">\n",
		   atom_type[type].name, datatype_str(type, dout));
	if (fetch)
		fprintf(f, "            <ptl_md type=\"%s\" md_data=\"%s\">\n",
			   atom_type[type].name, datatype_str(type, din));

	if (match)
		fprintf(f, "            <ptl_%s atom_op=\"%s\" atom_type=\"%s\" length=\"%d\" match=\"0x%" PRIu64 "\" target_id=\"SELF\"/>\n",
			   ptl_op, atom_op_name[op], atom_type[type].name, length, match_bits);
	else
		fprintf(f, "            <ptl_%s atom_op=\"%s\" atom_type=\"%s\" length=\"%d\" target_id=\"SELF\"/>\n",
			   ptl_op, atom_op_name[op], atom_type[type].name, length);

	/* TODO replace with an event */
	fprintf(f, "            <msleep count=\"10\"/>\n");

	if (fetch) {
		/* check to see that din data has changed */
		fprintf(f, "            <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
			   length, atom_type[type].name, datatype_str(type, tgt));
		fprintf(f, "            <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
			   atom_type[type].size, atom_type[type].name, length, datatype_str(type, din));

		fprintf(f, "            </ptl_md>\n");
	}

	/* check to see that dout data has not changed */
	fprintf(f, "            <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
		   length, atom_type[type].name, datatype_str(type, dout));
	fprintf(f, "            <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
		   atom_type[type].size, atom_type[type].name, length, datatype_str(type, dout));

	fprintf(f, "          </ptl_md>\n");

	/* check to see that target data has changed */
	if (match) {
		fprintf(f, "          <check length=\"%d\" type=\"%s\" me_data=\"%s\"/>\n",
			   length, atom_type[type].name, datatype_str(type, z));
		fprintf(f, "          <check length=\"%d\" type=\"%s\" offset=\"%d\" me_data=\"%s\"/>\n",
			   atom_type[type].size, atom_type[type].name, length, datatype_str(type, tgt));
	} else {
		fprintf(f, "          <check length=\"%d\" type=\"%s\" le_data=\"%s\"/>\n",
			   length, atom_type[type].name, datatype_str(type, z));
		fprintf(f, "          <check length=\"%d\" type=\"%s\" offset=\"%d\" le_data=\"%s\"/>\n",
			   atom_type[type].size, atom_type[type].name, length, datatype_str(type, tgt));
	}

	if (match)
		fprintf(f, "        </ptl_me>\n");
	else
		fprintf(f, "        </ptl_le>\n");

	fprintf(f, "      </ptl_pt>\n");
	fprintf(f, "    </ptl_ni>\n");
	fprintf(f, "  </ptl>\n");

	fprintf(f, "</test>\n");

	if (to_files) {
		fclose(f);
	}
}

int main(int argc, char *argv[])
{
	int err;
	int match;
	uint64_t match_bits;
	int op;
	int type;
	datatype_t tgt, din, dout, z;
	int length;
	time_t cur_time = time(NULL);
	int fetch;

	seed = cur_time;

	err = arg_process(argc, argv);
	if (err)
		goto done;

	srandom(seed);

	/* Generate one of each kind. */
	for (fetch =0; fetch <= 1; fetch ++) {

		for (op=PTL_MIN; op<=PTL_BXOR; op++) {

			for (type = PTL_INT8_T; type <= PTL_LONG_DOUBLE_COMPLEX; type ++) {

				if (!check_op_type_valid(op, type))
					continue;

				match = random() & 1;
				match_bits = random();
				match_bits = match_bits << 32 | random();

				tgt = get_data(type);
				dout = get_data(type);
				din = get_data(type);
				z = get_result(tgt, dout, op, type);

				length = random() % (max_length/atom_type[type].size);
				length = (length + 1)*atom_type[type].size;

				generate_case(op, type, match, match_bits, din, dout, tgt,
							  z, length, fetch);
			}
		}
	}

	err = 0;

 done:
	return err;
}

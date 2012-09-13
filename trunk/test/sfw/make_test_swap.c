/*
 * make_test_swap.c - generate random swap test cases
 */
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

static datatype_t get_result(datatype_t d, datatype_t t, datatype_t o, int op, int type)
{
	datatype_t z;

	z.u64 = 0;

	switch(op) {
	case PTL_SWAP:
		z = d;
		break;
	case PTL_CSWAP:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = (o.s8 == t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = (o.u8 == t.u8) ? d.u8 : t.u8;
			break;
		case PTL_INT16_T:
			z.s16 = (o.s16 == t.s16) ? d.s16 : t.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = (o.u16 == t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT32_T:
			z.s32 = (o.s32 == t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = (o.u32 == t.u32) ? d.u32 : t.u32;
			break;
		case PTL_INT64_T:
			z.s64 = (o.s64 == t.s64) ? d.s64 : t.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = (o.u64 == t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f == t.f) ? d.f : t.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc = (o.fc == t.fc) ? d.fc : t.fc;
			break;
		case PTL_DOUBLE:
			z.d = (o.d == t.d) ? d.d : t.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc = (o.dc == t.dc) ? d.dc : t.dc;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = (o.ld == t.ld) ? d.ld : t.ld;
			break;
		case PTL_LONG_DOUBLE_COMPLEX:
			z.ldc = (o.ldc == t.ldc) ? d.ldc : t.ldc;
			break;
		}
		break;
	case PTL_CSWAP_NE:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = (o.s8 != t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = (o.u8 != t.u8) ? d.u8 : t.u8;
			break;
		case PTL_INT16_T:
			z.s16 = (o.s16 != t.s16) ? d.s16 : t.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = (o.u16 != t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT32_T:
			z.s32 = (o.s32 != t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = (o.u32 != t.u32) ? d.u32 : t.u32;
			break;
		case PTL_INT64_T:
			z.s64 = (o.s64 != t.s64) ? d.s64 : t.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = (o.u64 != t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f != t.f) ? d.f : t.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc = (o.fc != t.fc) ? d.fc : t.fc;
			break;
		case PTL_DOUBLE:
			z.d = (o.d != t.d) ? d.d : t.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc = (o.dc != t.dc) ? d.dc : t.dc;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = (o.ld != t.ld) ? d.ld : t.ld;
			break;
		case PTL_LONG_DOUBLE_COMPLEX:
			z.ldc = (o.ldc != t.ldc) ? d.ldc : t.ldc;
			break;
		}
		break;
	case PTL_CSWAP_LE:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = (o.s8 <= t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = (o.u8 <= t.u8) ? d.u8 : t.u8;
			break;
		case PTL_INT16_T:
			z.s16 = (o.s16 <= t.s16) ? d.s16 : t.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = (o.u16 <= t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT32_T:
			z.s32 = (o.s32 <= t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = (o.u32 <= t.u32) ? d.u32 : t.u32;
			break;
		case PTL_INT64_T:
			z.s64 = (o.s64 <= t.s64) ? d.s64 : t.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = (o.u64 <= t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f <= t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d <= t.d) ? d.d : t.d;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = (o.ld <= t.ld) ? d.ld : t.ld;
			break;
		}
		break;
	case PTL_CSWAP_LT:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = (o.s8 < t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = (o.u8 < t.u8) ? d.u8 : t.u8;
			break;
		case PTL_INT16_T:
			z.s16 = (o.s16 < t.s16) ? d.s16 : t.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = (o.u16 < t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT32_T:
			z.s32 = (o.s32 < t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = (o.u32 < t.u32) ? d.u32 : t.u32;
			break;
		case PTL_INT64_T:
			z.s64 = (o.s64 < t.s64) ? d.s64 : t.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = (o.u64 < t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f < t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d < t.d) ? d.d : t.d;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = (o.ld < t.ld) ? d.ld : t.ld;
			break;
		}
		break;
	case PTL_CSWAP_GE:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = (o.s8 >= t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = (o.u8 >= t.u8) ? d.u8 : t.u8;
			break;
		case PTL_INT16_T:
			z.s16 = (o.s16 >= t.s16) ? d.s16 : t.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = (o.u16 >= t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT32_T:
			z.s32 = (o.s32 >= t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = (o.u32 >= t.u32) ? d.u32 : t.u32;
			break;
		case PTL_INT64_T:
			z.s64 = (o.s64 >= t.s64) ? d.s64 : t.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = (o.u64 >= t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f >= t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d >= t.d) ? d.d : t.d;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = (o.ld >= t.ld) ? d.ld : t.ld;
			break;
		}
		break;
	case PTL_CSWAP_GT:
		switch (type) {
		case PTL_INT8_T:
			z.s8 = (o.s8 > t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UINT8_T:
			z.u8 = (o.u8 > t.u8) ? d.u8 : t.u8;
			break;
		case PTL_INT16_T:
			z.s16 = (o.s16 > t.s16) ? d.s16 : t.s16;
			break;
		case PTL_UINT16_T:
			z.u16 = (o.u16 > t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT32_T:
			z.s32 = (o.s32 > t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT32_T:
			z.u32 = (o.u32 > t.u32) ? d.u32 : t.u32;
			break;
		case PTL_INT64_T:
			z.s64 = (o.s64 > t.s64) ? d.s64 : t.s64;
			break;
		case PTL_UINT64_T:
			z.u64 = (o.u64 > t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f > t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d > t.d) ? d.d : t.d;
			break;
		case PTL_LONG_DOUBLE:
			z.ld = (o.ld > t.ld) ? d.ld : t.ld;
			break;
		}
		break;
	case PTL_MSWAP:
		switch (type) {
		case PTL_INT8_T:
		case PTL_UINT8_T:
			z.s8 = (o.s8 & d.s8) | (~o.s8 & t.s8);
			break;
		case PTL_INT16_T:
		case PTL_UINT16_T:
			z.s16 = (o.s16 & d.s16) | (~o.s16 & t.s16);
			break;
		case PTL_INT32_T:
		case PTL_UINT32_T:
			z.s32 = (o.s32 & d.s32) | (~o.s32 & t.s32);
			break;
		case PTL_INT64_T:
		case PTL_UINT64_T:
			z.s64 = (o.s64 & d.s64) | (~o.s64 & t.s64);
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
	printf("make_test_swap [OPTIONS] > output_file\n");
	printf("\n");
	printf("SYNOPSYS:\n");
	printf("Generate portals4 test cases for the PtlSwap operation.\n");
	printf("These cases cover all the valid combinations.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("    --help | -h                   print this message\n");
	printf("    --seed | -s <seed>            set random number seed (default time())\n");
	printf("    --max_length | -m <length>    set max message length (>= 8) (default 32)\n");
	printf("    --write | -w                  output to separate files\n");
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
		{"write", 0, NULL, 'w'},
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
						  datatype_t opr,
						  datatype_t z,
						  int length)
{
	FILE *f;

	if (to_files) {
		char fname[1000];

		casenum ++;
		sprintf(fname, "temp/test_swap_all-%03d.xml", casenum);
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
	fprintf(f, "\n");
	fprintf(f, "		seed = %d\n", seed);
	fprintf(f, "		max_length = %d\n", max_length);
	fprintf(f, "-->\n");

	fprintf(f, "<test>\n");

	fprintf(f, "  <desc>Test swap %s/%s</desc>\n",
			atom_op_name[op], atom_type[type].name);
	fprintf(f, "  <ptl>\n");

	if (match)
		fprintf(f, "    <ptl_ni ni_opt=\"MATCH PHYSICAL\">\n");
	else
		fprintf(f, "    <ptl_ni ni_opt=\"NO_MATCH PHYSICAL\">\n");

	fprintf(f, "      <ptl_pt>\n");

	if (match)
		fprintf(f, "        <ptl_me me_opt=\"OP_PUT OP_GET\" type=\"%s\" me_data=\"%s\" me_match=\"0x%" PRIu64 "\">\n",
				atom_type[type].name, datatype_str(type, tgt), match_bits);
	else
		fprintf(f, "        <ptl_le le_opt=\"OP_PUT OP_GET\" type=\"%s\" le_data=\"%s\">\n",
				atom_type[type].name, datatype_str(type, tgt));

	fprintf(f, "          <ptl_md type=\"%s\" md_data=\"%s\">\n",
			atom_type[type].name, datatype_str(type, dout));
	fprintf(f, "            <ptl_md type=\"%s\" md_data=\"%s\">\n",
			atom_type[type].name, datatype_str(type, din));

	fprintf(f, "              <ptl_swap atom_op=\"%s\" atom_type=\"%s\" length=\"%d\" operand=\"%s\"",
			atom_op_name[op], atom_type[type].name, length, datatype_str(type, opr));

	if (match)
		fprintf(f, " match=\"0x%" PRIu64 "\"", match_bits);

	fprintf(f, " target_id=\"SELF\"/>\n");

	fprintf(f, "              <msleep count=\"10\"/>\n");

	fprintf(f, "              <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
			length, atom_type[type].name, datatype_str(type, tgt));
	fprintf(f, "              <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
			atom_type[type].size, atom_type[type].name, length, datatype_str(type, din));

	fprintf(f, "            </ptl_md>\n");

	fprintf(f, "            <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
			length, atom_type[type].name, datatype_str(type, dout));
	fprintf(f, "            <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
			atom_type[type].size, atom_type[type].name, length, datatype_str(type, dout));

	fprintf(f, "          </ptl_md>\n");

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
	datatype_t din, dout, tgt, opr, z;
	int length;

	seed = time(NULL);

	err = arg_process(argc, argv);
	if (err)
		goto done;

	srandom(seed);

	/* Generate one of each kind. */
	for (op=PTL_SWAP; op<=PTL_MSWAP; op++) {

		for (type = PTL_INT8_T; type <= PTL_LONG_DOUBLE_COMPLEX; type ++) {

			if (!check_op_type_valid(op, type))
				continue;

			match = random() & 1;
			match_bits = random();
			match_bits = match_bits << 32 | random();

			din = get_data(type);
			dout = get_data(type);
			tgt = get_data(type);
			opr = get_data(type);

			if (op == PTL_SWAP) {
				length = random() % (max_length/atom_type[type].size);
				length = (length + 1)*atom_type[type].size;
			} else {
				length = atom_type[type].size;
			}

			/* Regular case. */
			z = get_result(dout, tgt, opr, op, type);
			generate_case(op, type, match, match_bits, din, dout, tgt, opr, z, length);

			if (op >= PTL_CSWAP_NE && op <= PTL_CSWAP_GT) {
				/* Need 2 more cases: lower/greater than, and
				 * equal. Since we have either lower/greater, we swap
				 * the arguments to get the other one. This assumes
				 * that the arguments are different. */

				/* Swap dout and tgt. */
				z = get_result(tgt, dout, opr, op, type);
				generate_case(op, type, match, match_bits, din, tgt, dout, opr, z, length);

				/* dout=tgt */
				z = get_result(tgt, tgt, opr, op, type);
				generate_case(op, type, match, match_bits, din, tgt, tgt, opr, z, length);
			}
		}
	}

	err = 0;

 done:
	return err;
}

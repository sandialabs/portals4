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
static unsigned int physical = 0;
static unsigned int count = 1;
static unsigned int max_length = 32;

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
		case PTL_CHAR:
			z.s8 = (o.s8 == t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UCHAR:
			z.u8 = (o.u8 == t.u8) ? d.u8 : t.u8;
			break;
		case PTL_SHORT:
			z.s16 = (o.s16 == t.s16) ? d.s16 : t.s16;
			break;
		case PTL_USHORT:
			z.u16 = (o.u16 == t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT:
			z.s32 = (o.s32 == t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT:
			z.u32 = (o.u32 == t.u32) ? d.u32 : t.u32;
			break;
		case PTL_LONG:
			z.s64 = (o.s64 == t.s64) ? d.s64 : t.s64;
			break;
		case PTL_ULONG:
			z.u64 = (o.u64 == t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f == t.f) ? d.f : t.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc[0] = (o.fc[0] == t.fc[0] && o.fc[1] == t.fc[1]) ? d.fc[0] : t.fc[0];
			z.fc[1] = (o.fc[0] == t.fc[0] && o.fc[1] == t.fc[1]) ? d.fc[1] : t.fc[1];
			break;
		case PTL_DOUBLE:
			z.d = (o.d == t.d) ? d.d : t.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc[0] = (o.dc[0] == t.dc[0] && o.dc[1] == t.dc[1]) ? d.dc[0] : t.dc[0];
			z.dc[1] = (o.dc[0] == t.dc[0] && o.dc[1] == t.dc[1]) ? d.dc[1] : t.dc[1];
			break;
		}
		break;
	case PTL_CSWAP_NE:
		switch (type) {
		case PTL_CHAR:
			z.s8 = (o.s8 != t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UCHAR:
			z.u8 = (o.u8 != t.u8) ? d.u8 : t.u8;
			break;
		case PTL_SHORT:
			z.s16 = (o.s16 != t.s16) ? d.s16 : t.s16;
			break;
		case PTL_USHORT:
			z.u16 = (o.u16 != t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT:
			z.s32 = (o.s32 != t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT:
			z.u32 = (o.u32 != t.u32) ? d.u32 : t.u32;
			break;
		case PTL_LONG:
			z.s64 = (o.s64 != t.s64) ? d.s64 : t.s64;
			break;
		case PTL_ULONG:
			z.u64 = (o.u64 != t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f != t.f) ? d.f : t.f;
			break;
		case PTL_FLOAT_COMPLEX:
			z.fc[0] = (o.fc[0] != t.fc[0] || o.fc[1] != t.fc[1]) ? d.fc[0] : t.fc[0];
			z.fc[1] = (o.fc[0] != t.fc[0] || o.fc[1] != t.fc[1]) ? d.fc[1] : t.fc[1];
			break;
		case PTL_DOUBLE:
			z.d = (o.d != t.d) ? d.d : t.d;
			break;
		case PTL_DOUBLE_COMPLEX:
			z.dc[0] = (o.dc[0] != t.dc[0] || o.dc[1] != t.dc[1]) ? d.dc[0] : t.dc[0];
			z.dc[1] = (o.dc[0] != t.dc[0] || o.dc[1] != t.dc[1]) ? d.dc[1] : t.dc[1];
			break;
		}
		break;
	case PTL_CSWAP_LE:
		switch (type) {
		case PTL_CHAR:
			z.s8 = (o.s8 <= t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UCHAR:
			z.u8 = (o.u8 <= t.u8) ? d.u8 : t.u8;
			break;
		case PTL_SHORT:
			z.s16 = (o.s16 <= t.s16) ? d.s16 : t.s16;
			break;
		case PTL_USHORT:
			z.u16 = (o.u16 <= t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT:
			z.s32 = (o.s32 <= t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT:
			z.u32 = (o.u32 <= t.u32) ? d.u32 : t.u32;
			break;
		case PTL_LONG:
			z.s64 = (o.s64 <= t.s64) ? d.s64 : t.s64;
			break;
		case PTL_ULONG:
			z.u64 = (o.u64 <= t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f <= t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d <= t.d) ? d.d : t.d;
			break;
		}
		break;
	case PTL_CSWAP_LT:
		switch (type) {
		case PTL_CHAR:
			z.s8 = (o.s8 < t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UCHAR:
			z.u8 = (o.u8 < t.u8) ? d.u8 : t.u8;
			break;
		case PTL_SHORT:
			z.s16 = (o.s16 < t.s16) ? d.s16 : t.s16;
			break;
		case PTL_USHORT:
			z.u16 = (o.u16 < t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT:
			z.s32 = (o.s32 < t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT:
			z.u32 = (o.u32 < t.u32) ? d.u32 : t.u32;
			break;
		case PTL_LONG:
			z.s64 = (o.s64 < t.s64) ? d.s64 : t.s64;
			break;
		case PTL_ULONG:
			z.u64 = (o.u64 < t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f < t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d < t.d) ? d.d : t.d;
			break;
		}
		break;
	case PTL_CSWAP_GE:
		switch (type) {
		case PTL_CHAR:
			z.s8 = (o.s8 >= t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UCHAR:
			z.u8 = (o.u8 >= t.u8) ? d.u8 : t.u8;
			break;
		case PTL_SHORT:
			z.s16 = (o.s16 >= t.s16) ? d.s16 : t.s16;
			break;
		case PTL_USHORT:
			z.u16 = (o.u16 >= t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT:
			z.s32 = (o.s32 >= t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT:
			z.u32 = (o.u32 >= t.u32) ? d.u32 : t.u32;
			break;
		case PTL_LONG:
			z.s64 = (o.s64 >= t.s64) ? d.s64 : t.s64;
			break;
		case PTL_ULONG:
			z.u64 = (o.u64 >= t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f >= t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d >= t.d) ? d.d : t.d;
			break;
		}
		break;
	case PTL_CSWAP_GT:
		switch (type) {
		case PTL_CHAR:
			z.s8 = (o.s8 > t.s8) ? d.s8 : t.s8;
			break;
		case PTL_UCHAR:
			z.u8 = (o.u8 > t.u8) ? d.u8 : t.u8;
			break;
		case PTL_SHORT:
			z.s16 = (o.s16 > t.s16) ? d.s16 : t.s16;
			break;
		case PTL_USHORT:
			z.u16 = (o.u16 > t.u16) ? d.u16 : t.u16;
			break;
		case PTL_INT:
			z.s32 = (o.s32 > t.s32) ? d.s32 : t.s32;
			break;
		case PTL_UINT:
			z.u32 = (o.u32 > t.u32) ? d.u32 : t.u32;
			break;
		case PTL_LONG:
			z.s64 = (o.s64 > t.s64) ? d.s64 : t.s64;
			break;
		case PTL_ULONG:
			z.u64 = (o.u64 > t.u64) ? d.u64 : t.u64;
			break;
		case PTL_FLOAT:
			z.f = (o.f > t.f) ? d.f : t.f;
			break;
		case PTL_DOUBLE:
			z.d = (o.d > t.d) ? d.d : t.d;
			break;
		}
		break;
	case PTL_MSWAP:
		switch (type) {
		case PTL_CHAR:
		case PTL_UCHAR:
			z.s8 = (o.s8 & d.s8) | (~o.s8 & t.s8);
			break;
		case PTL_SHORT:
		case PTL_USHORT:
			z.s16 = (o.s16 & d.s16) | (~o.s16 & t.s16);
			break;
		case PTL_INT:
		case PTL_UINT:
			z.s32 = (o.s32 & d.s32) | (~o.s32 & t.s32);
			break;
		case PTL_LONG:
		case PTL_ULONG:
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
	printf("Generate random portals4 test cases for the PtlSwap operation to standard out.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("	--help | -h			print this message\n");
	printf("	--seed | -s		seed	set random number seed (default time())\n");
	printf("	--count | -c		count	set number of test cases (default 1)\n");
	printf("	--max_length | -m	length	set max message length (>= 8) (default 32)\n");
	printf("	--physical | -p			physical use physical NI (logical NI)\n");
	printf("\n");
}

static int arg_process(int argc, char *argv[])
{
	int c;
	int option_index = 0;
	static char *opt_string = "hps:c:m:";
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"seed", 1, NULL, 's'},
		{"count", 1, NULL, 'c'},
		{"max_length", 1, NULL, 'm'},
		{"physical", 0, NULL, 'p'},
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

		case 'c':
			count = strtol(optarg, NULL, 0);
			break;

		case 'p':
			physical = 1;
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
	datatype_t din, dout, tgt, opr, z;
	int length;
	time_t cur_time = time(NULL);

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

		do {
			op = (random() % (PTL_OP_LAST - PTL_SWAP)) + PTL_SWAP;
			type = random() % PTL_DATATYPE_LAST;
		} while ((op == PTL_MSWAP && type >= PTL_FLOAT) ||
			 (op > PTL_CSWAP_NE && (type == PTL_FLOAT_COMPLEX ||
						type == PTL_DOUBLE_COMPLEX)));

		din = get_data(type);
		dout = get_data(type);
		tgt = get_data(type);
		opr = get_data(type);
		z = get_result(dout, tgt, opr, op, type);

		if (op == PTL_SWAP) {
			length = random() % (max_length/atom_type[type].size);
			length = (length + 1)*atom_type[type].size;
		} else {
			length = atom_type[type].size;
		}

		printf("  <subtest>\n");
		printf("    <desc>Test swap %s/%s</desc>\n",
			atom_op_name[op], atom_type[type].name);
		printf("    <ptl>\n");

		if (match)
			printf("      <ptl_ni ni_opt=\"MATCH PHYSICAL\">\n");
		else
			printf("      <ptl_ni ni_opt=\"NO_MATCH PHYSICAL\">\n");

		printf("        <ptl_pt>\n");

		if (match)
			printf("          <ptl_me me_opt=\"OP_PUT OP_GET\" type=\"%s\" me_data=\"%s\" me_match=\"0x%" PRIu64 "\">\n",
				atom_type[type].name, datatype_str(type, tgt), match_bits);
		else
			printf("          <ptl_le le_opt=\"OP_PUT OP_GET\" type=\"%s\" le_data=\"%s\">\n",
				atom_type[type].name, datatype_str(type, tgt));

		printf("            <ptl_md type=\"%s\" md_data=\"%s\">\n",
			atom_type[type].name, datatype_str(type, dout));
		printf("              <ptl_md type=\"%s\" md_data=\"%s\">\n",
			atom_type[type].name, datatype_str(type, din));

		printf("                <ptl_swap atom_op=\"%s\" atom_type=\"%s\" length=\"%d\" operand=\"%s\"",
			atom_op_name[op], atom_type[type].name, length, datatype_str(type, opr));

		if (match)
			printf(" match=\"0x%" PRIu64 "\"", match_bits);

		printf(" target_id=\"SELF\"/>\n");

		printf("                <msleep count=\"10\"/>\n");

		printf("                <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
			length, atom_type[type].name, datatype_str(type, tgt));
		printf("                <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
			atom_type[type].size, atom_type[type].name, length, datatype_str(type, din));

		printf("              </ptl_md>\n");

		printf("              <check length=\"%d\" type=\"%s\" md_data=\"%s\"/>\n",
			length, atom_type[type].name, datatype_str(type, dout));
		printf("              <check length=\"%d\" type=\"%s\" offset=\"%d\" md_data=\"%s\"/>\n",
			atom_type[type].size, atom_type[type].name, length, datatype_str(type, dout));

		printf("            </ptl_md>\n");

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

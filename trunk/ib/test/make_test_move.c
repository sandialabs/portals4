#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <inttypes.h>
#include "../include/portals4.h"
#include "data.h"

typedef struct param {
	unsigned int	seed;
	unsigned int	physical;
	unsigned int	count;
	unsigned int	max_length;
	unsigned int	op_mask;
	int		match;
	uint64_t	match_bits;
	int		atom_op;
	int		type;
	datatype_t	tgt;
	datatype_t	din;
	datatype_t	dout;
	datatype_t	z;
	int		length;
	op_t		ptl_op;
	char		*me_opt;
	int		indent;
} param_t;

char *ptl_op_name[] = {
	[OP_PUT]	= "ptl_put",
	[OP_GET]	= "ptl_get",
	[OP_ATOMIC]	= "ptl_atomic",
	[OP_FETCH]	= "ptl_fetch",
	[OP_SWAP]	= "ptl_swap",
	[5]		= "bad5",
	[6]		= "bad6",
	[7]		= "bad7",
	[8]		= "bad8",
	[9]		= "bad9",
};

param_t param = {
	.count		= 1,
	.max_length	= 32,
	.op_mask	= (1 << OP_GET) | (1 << OP_PUT),
};

char *indent[] = {
	"",
	"  ",
	"    ",
	"      ",
	"        ",
	"          ",
	"            ",
	"              ",
	"                ",
	"                  ",
	"                    ",
};

datatype_t get_result(datatype_t x, datatype_t y, int op, int type)
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
		case PTL_DOUBLE:
			z.d = x.d + y.d;
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
		case PTL_DOUBLE:
			z.d = x.d * y.d;
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

void usage()
{
	printf("usage:\n");
	printf("\n");
	printf("make_test_move [OPTIONS] > output_file\n");
	printf("\n");
	printf("SYNOPSYS:\n");
	printf("Generate random portals4 test cases for PtlPut/Get operation to standard out.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("	-h | --help			  print this message\n");
	printf("	-s | --seed		seed	  set random number seed (default time())\n");
	printf("	-p | --physical		physical  use physical NI (logical NI)\n");
	printf("	-c | --count		count	  set number of test cases (default 1)\n");
	printf("	-m | --max_length	length	  set max message length (>= 8) (default 32)\n");
	printf("	-A | --atomic			  include PtlAtomic\n");
	printf("	-F | --fetch			  include PtlFetchAtomic\n");
	printf("	-S | --swap			  include Ptlswap\n");
	printf("\n");
}

int arg_process(int argc, char *argv[])
{
	int c;
	int option_index = 0;
	static char *opt_string = "AFShps:c:m:";
	static struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"seed", 1, 0, 's'},
		{"physical", 0, 0, 'p'},
		{"count", 1, 0, 'c'},
		{"max_length", 1, 0, 'm'},
		{"atomic", 0, 0, 'A'},
		{"fetch", 0, 0, 'F'},
		{"swap", 0, 0, 'S'},
		{0, 0, 0, 0}
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
			param.seed = strtol(optarg, NULL, 0);
			break;

		case 'p':
			param.physical = 1;
			break;

		case 'c':
			param.count = strtol(optarg, NULL, 0);
			break;

		case 'm':
			param.max_length = strtol(optarg, NULL, 0);
			if (param.max_length < 8) {
				printf("max_length too small\n");
				return 1;
			}
			break;

		case 'A':
			param.op_mask |= 1 << OP_ATOMIC;
			break;

		case 'F':
			param.op_mask |= 1 << OP_FETCH;
			break;

		case 'S':
			param.op_mask |= 1 << OP_SWAP;
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

void get_random_param(param_t *p)
{
	p->match = random() & 1;
	p->match_bits = random();
	p->match_bits = p->match_bits << 32 | random();

	do {
		p->ptl_op = random() % OP_LAST;
	} while(!(p->op_mask & (1 << p->ptl_op)));

	if (p->ptl_op == OP_ATOMIC || p->ptl_op == OP_FETCH) {
		do {
			p->atom_op = random() % (PTL_BXOR + 1);
			p->type = random() % (PTL_DOUBLE + 1);
		} while(p->atom_op >= PTL_LOR && p->type >= PTL_FLOAT);
	}

	else if (p->ptl_op == OP_SWAP) {
		do {
			p->atom_op = (random() % (_PTL_OP_LAST - PTL_SWAP)) + PTL_SWAP;
			p->type = random() % _PTL_DATATYPE_LAST;
		} while (p->atom_op == PTL_MSWAP && p->type >= PTL_FLOAT);
	}

	else {
		p->atom_op = PTL_MIN;
		p->type = PTL_UCHAR;
	}

	p->length = random() % (p->max_length/atom_type[p->type].size);
	p->length = (p->length + 1)*atom_type[p->type].size;

	p->tgt = get_data(p->type);
	p->dout = get_data(p->type);
	p->din = get_data(p->type);
	p->z = get_result(p->tgt, p->dout, p->atom_op, p->type);

	p->me_opt = "OP_GET OP_PUT";
}

void start_test(param_t *p, int argc, char *argv[], time_t cur_time)
{
	int i;

	printf("<?xml version=\"1.0\"?>\n");

	printf("<!--\n");
	printf("	file generated %s", ctime(&cur_time));
	printf("		command =");
	for (i = 0; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");
	printf("		seed = %d\n", p->seed);
	printf("		count = %d\n", p->count);
	printf("		max_length = %d\n", p->max_length);
	printf("-->\n");

	printf("%s<test>\n", indent[p->indent++]);
}

void end_test(param_t *p)
{
	printf("%s</test>\n", indent[--p->indent]);
}

void start_subtest(param_t *p)
{
	printf("%s<subtest>\n", indent[p->indent++]);
	printf("%s<desc>Test %s", indent[p->indent],
	       ptl_op_name[p->ptl_op]);
	if (p->ptl_op == OP_ATOMIC || p->ptl_op == OP_FETCH || p->ptl_op == OP_SWAP)
		printf(" %s/%s", atom_op_name[p->atom_op], atom_type[p->type].name);
	printf(" length = %d</desc>\n", p->length);
	printf("%s<ptl>\n", indent[p->indent++]);
}

void end_subtest(param_t *p)
{
	printf("%s</ptl>\n", indent[--p->indent]);
	printf("%s</subtest>\n", indent[--p->indent]);
}

void start_ni(param_t *p)
{
	if (p->physical) {
		printf("%s<ptl_ni ni_opt=\"%s PHYSICAL\">\n", indent[p->indent++],
		       p->match ? "MATCH" : "NO_MATCH");
		printf("%s<ompi_rt>\n", indent[p->indent++]);
	} else {
		printf("%s<ptl_ni ni_opt=\"%s PHYSICAL\">\n", indent[p->indent++],
		       p->match ? "MATCH" : "NO_MATCH");
		printf("%s<ompi_rt>\n", indent[p->indent++]);
		printf("%s<ptl_ni ni_opt=\"%s LOGICAL\">\n", indent[p->indent++],
		       p->match ? "MATCH" : "NO_MATCH");
	}
	printf("%s<ptl_pt>\n", indent[p->indent++]);
}

void end_ni(param_t *p)
{
	printf("%s</ptl_pt>\n", indent[--p->indent]);
	if (p->physical) {
		printf("%s</ompi_rt>\n", indent[--p->indent]);
		printf("%s</ptl_ni>\n", indent[--p->indent]);
	} else {
		printf("%s</ptl_ni>\n", indent[--p->indent]);
		printf("%s</ompi_rt>\n", indent[--p->indent]);
		printf("%s</ptl_ni>\n", indent[--p->indent]);
	}
}

void start_xe(param_t *p)
{
	if (p->match) {
		printf("%s<ptl_me me_opt=\"%s\" me_match=\"0x%"
		       PRIu64 "\" type=\"%s\" me_data=\"%s\">\n",
		       indent[p->indent++], p->me_opt, p->match_bits,
		       atom_type[p->type].name, datatype_str(p->type, p->tgt));
	} else {
		printf("%s<ptl_le le_opt=\"%s\" type=\"%s\" le_data=\"%s\">\n",
		       indent[p->indent++], p->me_opt, atom_type[p->type].name,
		       datatype_str(p->type, p->tgt));
	}
}

void end_xe(param_t *p)
{
	if (p->match)
		printf("%s</ptl_me>\n", indent[--p->indent]);
	else
		printf("%s</ptl_le>\n", indent[--p->indent]);
}

void start_md(param_t *p)
{
	if (p->ptl_op == OP_PUT || p->ptl_op == OP_ATOMIC ||
	    p->ptl_op == OP_FETCH || p->ptl_op == OP_SWAP) {
		printf("%s<ptl_md type=\"%s\" md_data=\"%s\">\n",
			indent[p->indent++], atom_type[p->type].name,
			datatype_str(p->type, p->dout));
	}
	if (p->ptl_op == OP_GET || p->ptl_op == OP_FETCH || p->ptl_op == OP_SWAP) {
		printf("%s<ptl_md type=\"%s\" md_data=\"%s\">\n",
			indent[p->indent++], atom_type[p->type].name,
			datatype_str(p->type, p->din));
	}
}

void end_md(param_t *p)
{
	if (p->ptl_op == OP_PUT || p->ptl_op == OP_ATOMIC ||
	    p->ptl_op == OP_FETCH || p->ptl_op == OP_SWAP) {
		printf("%s</ptl_md>\n", indent[--p->indent]);
	}

	if (p->ptl_op == OP_GET || p->ptl_op == OP_FETCH || p->ptl_op == OP_SWAP) {
		printf("%s</ptl_md>\n", indent[--p->indent]);
	}
}

void move_op(param_t *p)
{
	if (p->match) {
		printf("%s<%s atom_op=\"%s\" atom_type=\"%s\" "
		       "length=\"%d\" match=\"0x%" PRIu64
		       "\" target_id=\"SELF\"/>\n",
		       indent[p->indent], ptl_op_name[p->ptl_op], atom_op_name[p->atom_op],
		       atom_type[p->type].name, p->length, p->match_bits);
	} else {
		printf("%s<ptl_%s atom_op=\"%s\" atom_type=\"%s\" "
		       "length=\"%d\" target_id=\"SELF\"/>\n",
		       indent[p->indent], ptl_op_name[p->ptl_op], atom_op_name[p->atom_op],
		       atom_type[p->type].name, p->length);
	}
}

void check_din(param_t *p)
{
	if (0) {
		printf("%s<check length=\"%d\" type=\"%s\" "
		       "md_data=\"%s\"/>\n",
		       indent[p->indent], p->length, atom_type[p->type].name,
		       datatype_str(p->type, p->tgt));
		printf("%s<check length=\"%d\" type=\"%s\" offset=\"%d\" "
		       "md_data=\"%s\"/>\n",
		       indent[p->indent], atom_type[p->type].size,
		       atom_type[p->type].name, p->length,
		       datatype_str(p->type, p->din));
	}
}

void check_dout(param_t *p)
{
	printf("%s<check length=\"%d\" type=\"%s\" "
	       "md_data=\"%s\"/>\n",
	       indent[p->indent], p->length, atom_type[p->type].name,
	       datatype_str(p->type, p->dout));
	printf("%s<check length=\"%d\" type=\"%s\" offset=\"%d\" "
	       "md_data=\"%s\"/>\n",
	       indent[p->indent], atom_type[p->type].size,
	       atom_type[p->type].name, p->length,
	       datatype_str(p->type, p->dout));
}

void check_tgt(param_t *p)
{
	if (p->match) {
		printf("%s<check length=\"%d\" type=\"%s\" "
		       "me_data=\"%s\"/>\n",
		       indent[p->indent], p->length,
		       atom_type[p->type].name, datatype_str(p->type, p->z));
		printf("%s<check length=\"%d\" type=\"%s\" "
		       "offset=\"%d\" me_data=\"%s\"/>\n",
		       indent[p->indent], atom_type[p->type].size,
		       atom_type[p->type].name, p->length,
		       datatype_str(p->type, p->tgt));
	} else {
		printf("%s<check length=\"%d\" type=\"%s\" "
		       "le_data=\"%s\"/>\n",
		       indent[p->indent], p->length, atom_type[p->type].name,
		       datatype_str(p->type, p->z));
		printf("%s<check length=\"%d\" type=\"%s\" "
		       "offset=\"%d\" le_data=\"%s\"/>\n",
		       indent[p->indent], atom_type[p->type].size,
		       atom_type[p->type].name, p->length,
		       datatype_str(p->type, p->tgt));
	}
}

int main(int argc, char *argv[])
{
	param_t *p = &param;
	int err;
	int i;
	time_t cur_time = time(NULL);

	p->seed = cur_time;

	err = arg_process(argc, argv);
	if (err)
		return err;

	srandom(p->seed);

	start_test(p, argc, argv, cur_time);

	for (i = 0; i < p->count; i++) {
		get_random_param(p);
		start_subtest(p);
		start_ni(p);
		start_xe(p);
		start_md(p);
		move_op(p);

		/* TODO replace with an event */
		printf("%s<msleep count=\"10\"/>\n", indent[p->indent]);

		check_din(p);
		check_dout(p);
		end_md(p);
		check_tgt(p);
		end_xe(p);
		end_ni(p);
		end_subtest(p);
	}

	end_test(p);
	return 0;
}

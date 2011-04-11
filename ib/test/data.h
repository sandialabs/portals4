/*
 * data.h - datatype utilities
 */
#ifndef DATA_H
#define DATA_H

typedef enum {
	/* from init to target */
	OP_PUT,
	OP_GET,
	OP_ATOMIC,
	OP_FETCH,
	OP_SWAP,

	/* from target to init */
	OP_DATA,
	OP_REPLY,
	OP_ACK,
	OP_CT_ACK,
	OP_OC_ACK,

	OP_LAST,
} op_t;

typedef union datatype {
	int8_t		s8;
	uint8_t		u8;
	int16_t		s16;
	uint16_t	u16;
	int32_t		s32;
	uint32_t	u32;
	int64_t		s64;
	uint64_t	u64;
	float		f;
	double		d;
} datatype_t;

extern char *atom_op_name[_PTL_OP_LAST];

typedef struct {
	char *name;
	int size;
} atom_type_t;

extern atom_type_t atom_type[];

datatype_t get_data(int type);

char *datatype_str(int type, datatype_t data);

#endif /* DATA_H */

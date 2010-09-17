#ifndef PTL_INTERNAL_HANDLES_H
#define PTL_INTERNAL_HANDLES_H

#define HANDLE_SELECTOR_BITS	3
#define HANDLE_NI_BITS		2
#define HANDLE_CODE_BITS	27

typedef union
{
    uint32_t i;
    struct
    {
#ifdef BITFIELD_ORDER_FORWARD
	unsigned int selector:HANDLE_SELECTOR_BITS;
	unsigned char ni:HANDLE_NI_BITS;
	unsigned int code:HANDLE_CODE_BITS;
#else
	unsigned int code:HANDLE_CODE_BITS;
	unsigned char ni:HANDLE_NI_BITS;
	unsigned int selector:HANDLE_SELECTOR_BITS;
#endif
    } s;
    ptl_handle_any_t a;
} ptl_internal_handle_converter_t;

#define HANDLE_NI_CODE 0
#define HANDLE_EQ_CODE 1
#define HANDLE_CT_CODE 2
#define HANDLE_MD_CODE 3
#define HANDLE_LE_CODE 4
#define HANDLE_ME_CODE 5

#endif

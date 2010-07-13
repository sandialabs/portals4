#ifndef PTL_INTERNAL_HANDLES_H
#define PTL_INTERNAL_HANDLES_H

typedef struct {
#ifdef BITFIELD_ORDER_FORWARD
    unsigned int selector:3;
    unsigned int ni:3;
    unsigned int code:26;
#else
    unsigned int code:26;
    unsigned int ni:3;
    unsigned int selector:3;
#endif
} ptl_handle_encoding_t;

#define HANDLE_NI_CODE 0
#define HANDLE_EQ_CODE 1
#define HANDLE_CT_CODE 2
#define HANDLE_MD_CODE 3
#define HANDLE_LE_CODE 4
#define HANDLE_ME_CODE 5

#endif

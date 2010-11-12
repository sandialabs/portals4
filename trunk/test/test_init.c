#include <portals4.h>

#include <stdio.h>                     /* for fprintf() */
#include <stdlib.h>                    /* for abort() */
#include <assert.h>

#define CHECK_RETURNVAL(x) do { int ret; \
    switch (ret = x) { \
        case PTL_OK: break; \
        case PTL_FAIL: fprintf(stderr, "=> %s returned PTL_FAIL (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        case PTL_NO_SPACE: fprintf(stderr, "=> %s returned PTL_NO_SPACE (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        case PTL_ARG_INVALID: fprintf(stderr, "=> %s returned PTL_ARG_INVALID (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        case PTL_NO_INIT: fprintf(stderr, "=> %s returned PTL_NO_INIT (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        default: fprintf(stderr, "=> %s returned failcode %i (line %u)\n", #x, ret, (unsigned int)__LINE__); abort(); break; \
    } } while (0)

int main(
    int argc,
    char *argv[])
{
    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(PtlInit());
    PtlFini();
    PtlFini();
    return 0;
}

/* vim:set expandtab: */

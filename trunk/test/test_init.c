#include <portals4.h>

#include <stdio.h>                     /* for fprintf() */
#include <stdlib.h>                    /* for abort() */
#include <assert.h>

#include "testing.h"

int main(
         int argc,
         char *argv[])
{
    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(PtlInit());
    PtlFini();
    PtlFini();
    printf("sizeof ptl_event_t = %lu\n", sizeof(ptl_event_t));
    return 0;
}

/* vim:set expandtab: */
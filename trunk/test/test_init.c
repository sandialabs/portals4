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
    return 0;
}

/* vim:set expandtab: */

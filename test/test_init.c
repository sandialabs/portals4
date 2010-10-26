#include <portals4.h>

#include <assert.h>

int main(
    int argc,
    char *argv[])
{
    assert(PtlInit() == PTL_OK);
    assert(PtlInit() == PTL_OK);
    PtlFini();
    PtlFini();
    return 0;
}
/* vim:set expandtab */

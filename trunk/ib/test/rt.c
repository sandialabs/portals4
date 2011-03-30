#include "ptl_test.h"

static long getenv_val(const char *name, unsigned int *val)
{
	char *endptr;
	unsigned long myval;
	char *str;

	str = getenv(name);
	if (!str)
		return PTL_FAIL;
	errno = 0;
	myval = strtoul(str, &endptr, 10);

	if ((errno == ERANGE && myval == ULONG_MAX) ||
		(errno != 0 && myval == 0))
		return PTL_FAIL;

	/* The value is valid, but it also has to fit in an unsigned int. */
	if (myval > UINT_MAX)
		return PTL_FAIL;

	*val = myval;

	return PTL_OK;
}

static int set_jid(struct node_info *info)
{
	int ret;

	ret = getenv_val("OMPI_MCA_orte_ess_jobid", &info->jid);

	if (debug)
		printf("setting info->jid = %d\n", info->jid);

	return ret;
}

int ompi_rt_init(struct node_info *info)
{
	int errs = 0;

	if (set_jid(info))
		errs++;

	return errs;
}

#include <ck_pr.h>
#include <limits.h>
#include <stdio.h>

#include "../../common.h"

int
main(void)
{
	unsigned int x;

	ck_pr_store_uint(&x, 0x10110);

	if (ck_cc_ffs(0) != 0)
		ck_error("ffs(0) = %d\n", ck_cc_ffs(0));
	if (ck_cc_ffs(4) != 3)
		ck_error("ffs(4) = %d\n", ck_cc_ffs(4));
	if (ck_cc_ffs(UINT_MAX) != 1)
		ck_error("ffs(UINT_MAX) = %d\n", ck_cc_ffs(UINT_MAX));
	if (ck_cc_ffs(x) != 5)
		ck_error("ffs(%u) = %d\n", x, ck_cc_ffs(x));

	if (ck_cc_ffs(x) != ck_cc_ffsl(x) ||
	    ck_cc_ffsl(x) != ck_cc_ffsll(x) ||
	    ck_cc_ffs(x) != ck_cc_ffsll(x)) {
		ck_error("    ffs = %d, ffsl = %d, ffsll = %d\n",
		    ck_cc_ffs(x), ck_cc_ffsl(x), ck_cc_ffsll(x));
	}

	if (ck_cc_ctz(x) != 4)
		ck_error("ctz = %d\n", ck_cc_ctz(x));

	if (ck_cc_popcount(x) != 3)
		ck_error("popcount = %d\n", ck_cc_popcount(x));

	return 0;
}

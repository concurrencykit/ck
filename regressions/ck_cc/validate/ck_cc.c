#include <ck_pr.h>
#include <stdio.h>

#include "../../common.h"

int
main(void)
{
	unsigned int x;

	ck_pr_store_uint(&x, 4);

	printf("     ffs = %d\n", ck_cc_ffs(x));
	printf("     clz = %d\n", ck_cc_clz(x));
	printf("     ctz = %d\n", ck_cc_ctz(x));
	printf("popcount = %d\n", ck_cc_popcount(x));
	return 0;
}

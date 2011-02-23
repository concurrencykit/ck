#include <ck_bytelock.h> 
#include <ck_spinlock.h> 
#include <inttypes.h> 
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"
 
#ifndef STEPS  
#define STEPS 30000000 
#endif 

LOCK_DEFINE;

int
main(void)
{
	CK_CC_UNUSED unsigned int nthr = 1;

	#ifdef LOCK_INIT
	LOCK_INIT;
	#endif

	#ifdef LOCK_STATE
	LOCK_STATE;
	#endif

	uint64_t s_b, e_b, i;

	s_b = rdtsc();
	for (i = 0; i < STEPS; ++i) {
		#ifdef LOCK
		LOCK;
		UNLOCK;
		LOCK;
		UNLOCK;
		LOCK;
		UNLOCK;
		LOCK;
		UNLOCK;
		#endif
	}
	e_b = rdtsc();
	printf("%15" PRIu64 "\n", (e_b - s_b) / 4 / STEPS);

	return (0);
}


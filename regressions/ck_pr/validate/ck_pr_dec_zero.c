#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <ck_pr.h>

#define EXPECT(ACTUAL, IS_ZERO, TYPE, INITIAL) do {			\
	TYPE expected = (TYPE)((TYPE)INITIAL - (TYPE)1);		\
	if ((ACTUAL) != expected) {					\
		printf("FAIL [ %" PRIx64" != %" PRIx64" ]\n",		\
		       (uint64_t)(ACTUAL),				\
		       (uint64_t)expected);				\
		exit(EXIT_FAILURE);					\
	}								\
									\
	if ((IS_ZERO) != ((ACTUAL) == 0)) {				\
		printf("FAIL [ %s != %s ]\n",				\
		       ((IS_ZERO) ? "true" : "false"),			\
		       (((ACTUAL) == 0) ? "true" : "false"));		\
		exit(EXIT_FAILURE);					\
	}								\
	} while (0)

#define TEST_ZERO(TYPE, SUFFIX) do {				\
		TYPE datum;					\
		bool is_zero;					\
								\
		datum = 0;					\
		ck_pr_dec_##SUFFIX##_zero(&datum, &is_zero);	\
		EXPECT(datum, is_zero, TYPE, 0);		\
								\
		datum = (TYPE)-1;				\
		ck_pr_dec_##SUFFIX##_zero(&datum, &is_zero);	\
		EXPECT(datum, is_zero, TYPE, -1);		\
								\
		datum = (TYPE)1;				\
		ck_pr_dec_##SUFFIX##_zero(&datum, &is_zero);	\
		EXPECT(datum, is_zero, TYPE, 1);		\
								\
		datum = (TYPE)2;				\
		ck_pr_dec_##SUFFIX##_zero(&datum, &is_zero);	\
		EXPECT(datum, is_zero, TYPE, 2);		\
	} while (0)

#define TEST_IS_ZERO(TYPE, SUFFIX) do {				 \
		TYPE datum;					 \
		bool is_zero;					 \
								 \
		datum = 0;					 \
		is_zero = ck_pr_dec_##SUFFIX##_is_zero(&datum);	 \
		EXPECT(datum, is_zero, TYPE, 0);		 \
								 \
		datum = (TYPE)-1;				 \
		is_zero = ck_pr_dec_##SUFFIX##_is_zero(&datum);	 \
		EXPECT(datum, is_zero, TYPE, -1);		 \
								 \
		datum = (TYPE)1;				 \
		is_zero = ck_pr_dec_##SUFFIX##_is_zero(&datum);	 \
		EXPECT(datum, is_zero, TYPE, 1);		 \
								 \
		datum = (TYPE)2;				 \
		is_zero = ck_pr_dec_##SUFFIX##_is_zero(&datum);	 \
		EXPECT(datum, is_zero, TYPE, 2);		 \
	} while (0)

#define TEST(TYPE, SUFFIX) do {			\
	TEST_ZERO(TYPE, SUFFIX);		\
	TEST_IS_ZERO(TYPE, SUFFIX);		\
} while (0)

int
main(void)
{

#ifdef CK_F_PR_DEC_64_ZERO
	TEST(uint64_t, 64);
#endif

#ifdef CK_F_PR_DEC_32_ZERO
	TEST(uint32_t, 32);
#endif

#ifdef CK_F_PR_DEC_16_ZERO
	TEST(uint16_t, 16);
#endif

#ifdef CK_F_PR_DEC_8_ZERO
	TEST(uint8_t, 8);
#endif

#ifdef CK_F_PR_DEC_UINT_ZERO
	TEST(unsigned int, uint);
#endif

#ifdef CK_F_PR_DEC_INT_ZERO
	TEST(int, int);
#endif

#ifdef CK_F_PR_DEC_CHAR_ZERO
	TEST(char, char);
#endif

	return (0);
}

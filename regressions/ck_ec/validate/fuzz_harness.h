#ifndef FUZZ_HARNESS_H
#define FUZZ_HARNESS_H
#include <assert.h>
#include <ck_stddef.h>
#include <ck_string.h>
#include <stdio.h>
#include <unistd.h>

#if defined(USE_LIBFUZZER)
#define TEST(function, examples)					\
	void LLVMFuzzerInitialize(int *argcp, char ***argvp);		\
	int LLVMFuzzerTestOneInput(const void *data, size_t n);		\
									\
	void LLVMFuzzerInitialize(int *argcp, char ***argvp)		\
	{								\
		static char size[128];					\
		static char *argv[1024];				\
		int argc = *argcp;					\
									\
		assert(argc < 1023);					\
									\
		int r = snprintf(size, sizeof(size),			\
				 "-max_len=%zu", sizeof(examples[0]));	\
		assert((size_t)r < sizeof(size));			\
									\
		memcpy(argv, *argvp, argc * sizeof(argv[0]));		\
		argv[argc++] = size;					\
									\
		*argcp = argc;						\
		*argvp = argv;						\
									\
		for (size_t i = 0;					\
		     i < sizeof(examples) / sizeof(examples[0]);	\
		     i++) {						\
			assert(function(&examples[i]) == 0);		\
		}							\
									\
		return;							\
	}								\
									\
	int LLVMFuzzerTestOneInput(const void *data, size_t n)		\
	{								\
		char buf[sizeof(examples[0])];				\
									\
		memset(buf, 0, sizeof(buf));				\
		if (n < sizeof(buf)) {					\
			memcpy(buf, data, n);				\
		} else {						\
			memcpy(buf, data, sizeof(buf));			\
		}							\
									\
		assert(function((const void *)buf) == 0);		\
		return 0;						\
	}
#elif defined(USE_AFL)
#define TEST(function, examples)					\
	int main(int argc, char **argv)					\
	{								\
		char buf[sizeof(examples[0])];				\
									\
		(void)argc;						\
		(void)argv;						\
		for (size_t i = 0;					\
		     i < sizeof(examples) / sizeof(examples[0]);	\
		     i++) {						\
			assert(function(&examples[i]) == 0);		\
		}							\
									\
									\
		while (__AFL_LOOP(10000)) {				\
			memset(buf, 0, sizeof(buf));			\
			read(0, buf, sizeof(buf));			\
									\
			assert(function((const void *)buf) == 0);	\
		}							\
									\
		return 0;						\
	}
#else
#define TEST(function, examples)					\
	int main(int argc, char **argv)					\
	{								\
		(void)argc;						\
		(void)argv;						\
									\
		for (size_t i = 0;					\
		     i < sizeof(examples) / sizeof(examples[0]);	\
		     i++) {						\
			assert(function(&examples[i]) == 0);		\
		}							\
									\
		return 0;						\
	}
#endif
#endif /* !FUZZ_HARNESS_H */

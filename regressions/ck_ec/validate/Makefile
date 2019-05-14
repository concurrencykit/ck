.PHONY: check clean distribution

FUZZER ?= none

FUZZ_CFLAGS ?=

# See http://gallium.inria.fr/blog/portable-conditionals-in-makefiles/ for
# the portable conditional technique below.
none_fuzz_cflags =
libfuzzer_fuzz_cflags = -DUSE_LIBFUZZER -fsanitize=fuzzer,memory,undefined

FUZZ_CFLAGS += ${${FUZZER}_fuzz_cflags}

OBJECTS = ck_ec_smoke_test 		\
	prop_test_timeutil_add		\
	prop_test_timeutil_add_ns	\
	prop_test_timeutil_cmp		\
	prop_test_timeutil_scale	\
	prop_test_value 			\
	prop_test_wakeup			\
	prop_test_slow_wakeup

all: $(OBJECTS)

check: all
	./ck_ec_smoke_test
        # the command line arguments are only consumed by libfuzzer.
	./prop_test_slow_wakeup -max_total_time=60
	./prop_test_timeutil_add -max_total_time=60
	./prop_test_timeutil_add_ns -max_total_time=60
	./prop_test_timeutil_cmp -max_total_time=60
	./prop_test_timeutil_scale -max_total_time=60
	./prop_test_value -max_total_time=60
	./prop_test_wakeup -max_total_time=60

quickfuzz: all
	./prop_test_slow_wakeup -max_total_time=5
	./prop_test_timeutil_add -max_total_time=5
	./prop_test_timeutil_add_ns -max_total_time=5
	./prop_test_timeutil_cmp -max_total_time=5
	./prop_test_timeutil_scale -max_total_time=5
	./prop_test_value -max_total_time=5
	./prop_test_wakeup -max_total_time=5

ck_ec_smoke_test: ../../../src/ck_ec.c ck_ec_smoke_test.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h
	$(CC) $(CFLAGS) -std=gnu11 ../../../src/ck_ec.c -o ck_ec_smoke_test ck_ec_smoke_test.c

prop_test_slow_wakeup: ../../../src/ck_ec.c prop_test_slow_wakeup.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h fuzz_harness.h
	$(CC) $(CFLAGS) $(FUZZ_CFLAGS) ../../../src/ck_ec.c -o prop_test_slow_wakeup prop_test_slow_wakeup.c

prop_test_timeutil_add: ../../../src/ck_ec.c prop_test_timeutil_add.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h fuzz_harness.h
	$(CC) $(CFLAGS) $(FUZZ_CFLAGS) ../../../src/ck_ec.c -o prop_test_timeutil_add prop_test_timeutil_add.c

prop_test_timeutil_add_ns: ../../../src/ck_ec.c prop_test_timeutil_add_ns.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h fuzz_harness.h
	$(CC) $(CFLAGS) $(FUZZ_CFLAGS) ../../../src/ck_ec.c -o prop_test_timeutil_add_ns prop_test_timeutil_add_ns.c

prop_test_timeutil_cmp: ../../../src/ck_ec.c prop_test_timeutil_cmp.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h fuzz_harness.h
	$(CC) $(CFLAGS) $(FUZZ_CFLAGS) ../../../src/ck_ec.c -o prop_test_timeutil_cmp prop_test_timeutil_cmp.c

prop_test_timeutil_scale: ../../../src/ck_ec.c prop_test_timeutil_scale.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h fuzz_harness.h
	$(CC) $(CFLAGS) $(FUZZ_CFLAGS) ../../../src/ck_ec.c -o prop_test_timeutil_scale prop_test_timeutil_scale.c

prop_test_value: ../../../src/ck_ec.c prop_test_value.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h fuzz_harness.h
	$(CC) $(CFLAGS) $(FUZZ_CFLAGS) ../../../src/ck_ec.c -o prop_test_value prop_test_value.c

prop_test_wakeup: ../../../src/ck_ec.c prop_test_wakeup.c ../../../src/ck_ec_timeutil.h ../../../include/ck_ec.h fuzz_harness.h
	$(CC) $(CFLAGS) $(FUZZ_CFLAGS) ../../../src/ck_ec.c -o prop_test_wakeup prop_test_wakeup.c

clean:
	rm -rf *~ *.o *.dSYM *.exe $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

.PHONY: clean distribution

OBJECTS=throughput

all: $(OBJECTS)

throughput: throughput.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o throughput throughput.c ../../../src/ck_barrier.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -lpthread 


.PHONY: clean distribution

OBJECTS=throughput

all: $(OBJECTS)

throughput: throughput.c ../../../include/ck_barrier.h
	$(CC) $(CFLAGS) -o throughput throughput.c -lpthread

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -O0 -ggdb


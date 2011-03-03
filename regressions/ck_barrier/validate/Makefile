.PHONY: clean distribution

OBJECTS=ck_barrier_centralized ck_barrier_combining

all: $(OBJECTS)

ck_barrier_centralized: ck_barrier_centralized.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o ck_barrier_centralized ck_barrier_centralized.c ../../../src/ck_barrier.c

ck_barrier_combining: ck_barrier_combining.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o ck_barrier_combining ck_barrier_combining.c ../../../src/ck_barrier.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -lpthread

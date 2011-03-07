.PHONY: clean distribution

OBJECTS=ck_barrier_centralized ck_barrier_combining ck_barrier_dissemination ck_barrier_tournament ck_barrier_mcs

all: $(OBJECTS)

ck_barrier_centralized: ck_barrier_centralized.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o ck_barrier_centralized ck_barrier_centralized.c ../../../src/ck_barrier.c

ck_barrier_combining: ck_barrier_combining.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o ck_barrier_combining ck_barrier_combining.c ../../../src/ck_barrier.c

ck_barrier_dissemination: ck_barrier_dissemination.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o ck_barrier_dissemination ck_barrier_dissemination.c ../../../src/ck_barrier.c

ck_barrier_tournament: ck_barrier_tournament.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o ck_barrier_tournament ck_barrier_tournament.c ../../../src/ck_barrier.c

ck_barrier_mcs: ck_barrier_mcs.c ../../../include/ck_barrier.h ../../../src/ck_barrier.c
	$(CC) $(CFLAGS) -o ck_barrier_mcs ck_barrier_mcs.c ../../../src/ck_barrier.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -lpthread

.PHONY: check clean distribution

OBJECTS=ck_ring_spsc ck_ring_spmc ck_ring_spmc_template
SIZE=16384
CFLAGS += -g2

all: $(OBJECTS)

check: all
	./ck_ring_spsc $(CORES) 1 $(SIZE)
	./ck_ring_spmc $(CORES) 1 $(SIZE)

ck_ring_spsc: ck_ring_spsc.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -o ck_ring_spsc ck_ring_spsc.c \
		../../../src/ck_barrier_centralized.c

ck_ring_spmc: ck_ring_spmc.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -g2  -o ck_ring_spmc ck_ring_spmc.c \
		../../../src/ck_barrier_centralized.c

ck_ring_spmc_template: ck_ring_spmc_template.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -g2  -o ck_ring_spmc_template ck_ring_spmc.c \
		../../../src/ck_barrier_centralized.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

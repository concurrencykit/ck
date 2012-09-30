.PHONY: check clean distribution

OBJECTS=ck_ring_spsc ck_ring_spsc_template ck_ring_mpmc

all: $(OBJECTS)

check: all
	./ck_ring_spsc $(CORES) 1 65536
	./ck_ring_spsc_template $(CORES) 1 65536
	./ck_ring_mpmc $(CORES) 1 65536

ck_ring_spsc_template: ck_ring_spsc_template.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -o ck_ring_spsc_template ck_ring_spsc_template.c

ck_ring_spsc: ck_ring_spsc.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -o ck_ring_spsc ck_ring_spsc.c

ck_ring_mpmc: ck_ring_mpmc.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -o ck_ring_mpmc ck_ring_mpmc.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

.PHONY: clean distribution

OBJECTS=latency

all: $(OBJECTS)

latency: latency.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -o latency latency.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

.PHONY: clean distribution

OBJECTS=latency

all: $(OBJECTS)

latency: latency.c ../../../include/ck_ring.h
	$(CC) $(CFLAGS) -o latency latency.c

clean:
	rm -rf *~ *.o *.exe $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

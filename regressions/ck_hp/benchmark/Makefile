.PHONY: clean distribution

OBJECTS=fifo_latency

all: $(OBJECTS) 

fifo_latency: fifo_latency.c
	$(CC) $(CFLAGS) -o fifo_latency ../../../src/ck_hp.c fifo_latency.c

clean:
	rm -rf *~ *.o *.dSYM $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-lpthread -D_GNU_SOURCE

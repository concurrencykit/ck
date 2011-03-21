.PHONY: clean distribution

OBJECTS=latency

all: $(OBJECTS) 

latency: latency.c
	$(CC) $(CFLAGS) -o latency latency.c

clean:
	rm -rf *~ *.o *.dSYM $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-pthread -D_GNU_SOURCE

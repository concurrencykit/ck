.PHONY: clean distribution

OBJECTS=latency

all: $(OBJECTS)

latency: latency.c
	$(CC) $(CFLAGS) -o latency latency.c -lpthread

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-ggdb -D_GNU_SOURCE

.PHONY: clean distribution

OBJECTS=order

all: $(OBJECTS)

order: order.c ../../../include/ck_bag.h ../../../src/ck_bag.c
	$(CC) $(CFLAGS) -o order order.c ../../../src/ck_bag.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

.PHONY: check clean distribution

OBJECTS=order

all: $(OBJECTS)

order: order.c ../../../include/ck_bag.h ../../../src/ck_bag.c ../../../src/ck_epoch.c
	$(CC) $(CFLAGS) -o order order.c ../../../src/ck_bag.c ../../../src/ck_epoch.c

check: all
	./order

clean:
	rm -rf *.dSYM *.exe *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

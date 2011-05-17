.PHONY: clean distribution

OBJECTS=ck_fifo_spsc ck_fifo_mpmc ck_fifo_spsc_iterator ck_fifo_mpmc_iterator

all: ck_fifo_spsc ck_fifo_mpmc ck_fifo_spsc_iterator ck_fifo_mpmc_iterator

ck_fifo_spsc: ck_fifo_spsc.c
	$(CC) $(CFLAGS) -o ck_fifo_spsc ck_fifo_spsc.c

ck_fifo_mpmc: ck_fifo_mpmc.c
	$(CC) $(CFLAGS) -o ck_fifo_mpmc ck_fifo_mpmc.c

ck_fifo_spsc_iterator: ck_fifo_spsc_iterator.c
	$(CC) $(CFLAGS) -o ck_fifo_spsc_iterator ck_fifo_spsc_iterator.c

ck_fifo_mpmc_iterator: ck_fifo_mpmc_iterator.c
	$(CC) $(CFLAGS) -o ck_fifo_mpmc_iterator ck_fifo_mpmc_iterator.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

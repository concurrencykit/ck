.PHONY: check clean distribution

OBJECTS=ck_hp_stack nbds_haz_test serial ck_hp_fifo

all: $(OBJECTS)

check: all
	./serial
	./ck_hp_stack $(CORES) 10 1
	./ck_hp_fifo $(CORES) 1 64000 10 
	./nbds_haz_test $(CORES) 10 1

ck_hp_stack: ../../../src/ck_hp.c ck_hp_stack.c ../../../include/ck_hp_stack.h
	$(CC) $(CFLAGS) ../../../src/ck_hp.c -o ck_hp_stack ck_hp_stack.c

ck_hp_fifo: ../../../src/ck_hp.c ck_hp_fifo.c ../../../include/ck_hp_fifo.h
	$(CC) $(CFLAGS) ../../../src/ck_hp.c -o ck_hp_fifo ck_hp_fifo.c

serial: ../../../src/ck_hp.c serial.c ../../../include/ck_hp_stack.h
	$(CC) $(CFLAGS) ../../../src/ck_hp.c -o serial serial.c

nbds_haz_test: ../../../src/ck_hp.c nbds_haz_test.c
	$(CC) $(CFLAGS) ../../../src/ck_hp.c -o nbds_haz_test nbds_haz_test.c

clean:
	rm -rf *~ *.o *.dSYM $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

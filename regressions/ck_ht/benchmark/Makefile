.PHONY: clean distribution

OBJECTS=serial parallel

all: $(OBJECTS)

serial: serial.c ../../../include/ck_ht.h ../../../src/ck_ht.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_ht.c

parallel: parallel.c ../../../include/ck_ht.h ../../../src/ck_ht.c ../../../src/ck_epoch.c
	$(CC) $(PTHREAD_CFLAGS) $(CFLAGS) -o parallel parallel.c ../../../src/ck_ht.c ../../../src/ck_epoch.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

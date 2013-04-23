.PHONY: clean distribution

OBJECTS=serial parallel_bytestring parallel_direct

all: $(OBJECTS)

serial: serial.c ../../../include/ck_ht.h ../../../src/ck_ht.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_ht.c

parallel_bytestring: parallel_bytestring.c ../../../include/ck_ht.h ../../../src/ck_ht.c ../../../src/ck_epoch.c
	$(CC) $(PTHREAD_CFLAGS) $(CFLAGS) -o parallel_bytestring parallel_bytestring.c ../../../src/ck_ht.c ../../../src/ck_epoch.c

parallel_direct: parallel_direct.c ../../../include/ck_ht.h ../../../src/ck_ht.c ../../../src/ck_epoch.c
	$(CC) $(PTHREAD_CFLAGS) $(CFLAGS) -o parallel_direct parallel_direct.c ../../../src/ck_ht.c ../../../src/ck_epoch.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE


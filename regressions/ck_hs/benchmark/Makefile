.PHONY: clean distribution

OBJECTS=serial parallel_bytestring

all: $(OBJECTS)

serial: serial.c ../../../include/ck_hs.h ../../../src/ck_hs.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_hs.c

parallel_bytestring: parallel_bytestring.c ../../../include/ck_hs.h ../../../src/ck_hs.c ../../../src/ck_epoch.c
	$(CC) $(PTHREAD_CFLAGS) $(CFLAGS) -o parallel_bytestring parallel_bytestring.c ../../../src/ck_hs.c ../../../src/ck_epoch.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

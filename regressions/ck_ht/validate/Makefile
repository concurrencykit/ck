.PHONY: check clean distribution

OBJECTS=serial

all: $(OBJECTS)

serial: serial.c ../../../include/ck_ht.h ../../../src/ck_ht.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_ht.c

check: all
	./serial

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

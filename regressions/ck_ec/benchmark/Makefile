.PHONY: check clean distribution

OBJECTS=ck_ec

all: $(OBJECTS)

ck_ec: ck_ec.c ../../../include/ck_ec.h
	$(CC) $(CFLAGS) ../../../src/ck_ec.c -o ck_ec ck_ec.c

check: all
	./ck_ec $(CORES) 1

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE


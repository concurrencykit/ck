.PHONY: clean distribution

OBJECTS=ck_ht

all: $(OBJECTS)

ck_ht: ck_ht.c ../../../include/ck_ht.h ../../../src/ck_ht.c
	$(CC) $(CFLAGS) -o ck_ht ck_ht.c ../../../src/ck_ht.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

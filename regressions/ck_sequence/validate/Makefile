
.PHONY: clean distribution

OBJECTS=ck_sequence

all: $(OBJECTS)

ck_sequence: ck_sequence.c ../../../include/ck_sequence.h
	$(CC) $(CFLAGS) -o ck_sequence ck_sequence.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=-pthread -D_GNU_SOURCE -ggdb

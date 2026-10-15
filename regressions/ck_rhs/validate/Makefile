.PHONY: check clean distribution

OBJECTS=probe_bound serial

all: $(OBJECTS)

serial: serial.c ../../../include/ck_rhs.h ../../../src/ck_rhs.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_rhs.c

# probe_bound.c includes ../../../src/ck_rhs.c directly; see the note there.
probe_bound: probe_bound.c ../../../include/ck_rhs.h ../../../src/ck_rhs.c
	$(CC) $(CFLAGS) -o probe_bound probe_bound.c

check: all
	./probe_bound
	./serial

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

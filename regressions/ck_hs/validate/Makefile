.PHONY: check clean distribution

OBJECTS=serial hs_init_opts

all: $(OBJECTS)

serial: serial.c ../../../include/ck_hs.h ../../../src/ck_hs.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_hs.c

hs_init_opts: hs_init_opts.c ../../../include/ck_hs.h ../../../src/ck_hs.c
	$(CC) $(CFLAGS) -o hs_init_opts hs_init_opts.c ../../../src/ck_hs.c

check: all
	./serial

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

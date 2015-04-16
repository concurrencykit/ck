.PHONY: check clean distribution

OBJECTS=ck_epc_section
HALF=`expr $(CORES) / 2`

all: $(OBJECTS)

check: all
	./ck_epc_section $(CORES) 1

ck_epc_section: ck_epc_section.c ../../../include/ck_epc.h ../../../src/ck_epc.c ../../../include/ck_epoch.h ../../../src/ck_epoch.c
	$(CC) $(CFLAGS) -o ck_epc_section ck_epc_section.c ../../../src/ck_epc.c ../../../src/ck_epoch.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

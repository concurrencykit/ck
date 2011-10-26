.PHONY: clean distribution

OBJECTS=ck_stack ck_stack_read

all: $(OBJECTS)

ck_stack_read: ck_stack_read.c ../../../include/ck_stack.h ../../../include/ck_epoch.h ../../../src/ck_epoch.c
	$(CC) $(CFLAGS) -o ck_stack_read ck_stack_read.c ../../../src/ck_epoch.c

ck_stack: ck_stack.c ../../../include/ck_stack.h ../../../include/ck_epoch.h ../../../src/ck_epoch.c
	$(CC) $(CFLAGS) -o ck_stack ck_stack.c ../../../src/ck_epoch.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

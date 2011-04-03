.PHONY: clean distribution

OBJECTS=ck_stack ck_stack_read

all: $(OBJECTS)

ck_stack_read: ck_stack_read.c ../../../include/ck_stack.h ../../../include/ck_epoch.h
	$(CC) $(CFLAGS) -o ck_stack_read ck_stack_read.c

ck_stack: ck_stack.c ../../../include/ck_stack.h ../../../include/ck_epoch.h
	$(CC) $(CFLAGS) -o ck_stack ck_stack.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=-pthread -D_GNU_SOURCE

.PHONY: clean distribution

OBJECTS=validate throughput

all: $(OBJECTS)

validate: validate.c ../../../include/ck_rwlock.h
	$(CC) $(CFLAGS) -o validate validate.c

throughput: throughput.c ../../../include/ck_rwlock.h
	$(CC) $(CFLAGS) -o throughput throughput.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

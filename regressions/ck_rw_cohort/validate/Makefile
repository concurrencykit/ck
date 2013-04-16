.PHONY: check clean distribution

OBJECTS=validate

all: $(OBJECTS)

validate: validate.c ../../../include/ck_rw_cohort.h
	$(CC) $(CFLAGS) -o validate validate.c -g

check: all
	./validate $(CORES) 1

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

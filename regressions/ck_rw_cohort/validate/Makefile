.PHONY: check clean distribution

OBJECTS=ck_neutral ck_rp ck_wp

all: $(OBJECTS)

ck_neutral: ck_neutral.c ../../../include/ck_rw_cohort.h
	$(CC) $(CFLAGS) -o ck_neutral ck_neutral.c -g

ck_rp: ck_rp.c ../../../include/ck_rw_cohort.h
	$(CC) $(CFLAGS) -o ck_rp ck_rp.c -g

ck_wp: ck_wp.c ../../../include/ck_rw_cohort.h
	$(CC) $(CFLAGS) -o ck_wp ck_wp.c -g

check: all
	./validate $(CORES) 1

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

.PHONY: clean

OBJECTS=ck_pr_cas_64 ck_pr_fas_64 ck_pr_cas_64_2 ck_pr_add_64 ck_pr_faa_64 ck_pr_neg_64 fp

all: $(OBJECTS)

fp: fp.c
	$(CC) $(CFLAGS) -o fp fp.c

ck_pr_cas_64_2: ck_pr_cas_64_2.c
	$(CC) $(CFLAGS) -o ck_pr_cas_64_2 ck_pr_cas_64_2.c -lm

ck_pr_cas_64: ck_pr_cas_64.c
	$(CC) $(CFLAGS) -o ck_pr_cas_64 ck_pr_cas_64.c -lm

ck_pr_fas_64: ck_pr_fas_64.c
	$(CC) $(CFLAGS) -o ck_pr_fas_64 ck_pr_fas_64.c -lm

ck_pr_add_64: ck_pr_add_64.c
	$(CC) $(CFLAGS) -o ck_pr_add_64 ck_pr_add_64.c -lm

ck_pr_faa_64: ck_pr_faa_64.c
	$(CC) $(CFLAGS) -o ck_pr_faa_64 ck_pr_faa_64.c -lm

ck_pr_neg_64: ck_pr_neg_64.c
	$(CC) $(CFLAGS) -o ck_pr_neg_64 ck_pr_neg_64.c -lm

clean:
	rm -rf *.dSYM *.exe *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

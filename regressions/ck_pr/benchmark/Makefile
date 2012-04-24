.PHONY: clean

all: ck_pr_cas_64 ck_pr_fas_64 ck_pr_cas_64_2

ck_pr_cas_64_2: ck_pr_cas_64_2.c
	$(CC) $(CFLAGS) -o ck_pr_cas_64_2 ck_pr_cas_64_2.c

ck_pr_cas_64: ck_pr_cas_64.c
	$(CC) $(CFLAGS) -o ck_pr_cas_64 ck_pr_cas_64.c

ck_pr_fas_64: ck_pr_fas_64.c
	$(CC) $(CFLAGS) -o ck_pr_fas_64 ck_pr_fas_64.c

clean:
	rm -rf ck_pr_cas_64 ck_pr_fas_64 ck_pr_cas_64_2 *.dSYM

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE -lm

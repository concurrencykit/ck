.PHONY: clean

all: validate

validate: validate.c ../../../include/ck_backoff.h
	$(CC) $(CFLAGS) -o validate validate.c

clean:
	rm -rf validate *.dSYM

include ../../../build/regressions.build

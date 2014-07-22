.PHONY: check clean distribution

OBJECTS=validate

all: $(OBJECTS)

validate: validate.c ../../../include/ck_cohort.h
	$(CC) $(CFLAGS) -o validate validate.c

check: all
ifeq ($(expr ($(CORES) / 2) > 0),$(expr 0))
	./validate 1 2 1
else
	./validate `expr $(CORES) / 2` 2 1
endif

clean:
	rm -rf *.dSYM *.exe *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

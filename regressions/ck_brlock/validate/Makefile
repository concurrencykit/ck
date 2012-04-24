.PHONY: clean distribution

OBJECTS=validate

all: $(OBJECTS)

validate: validate.c ../../../include/ck_brlock.h
	$(CC) $(CFLAGS) -o validate validate.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

.PHONY: clean distribution

OBJECTS=validate 

all: $(OBJECTS)

validate: validate.c ../../../include/ck_barrier.h
	$(CC) $(CFLAGS) -o validate validate.c -lpthread

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -O0 -ggdb

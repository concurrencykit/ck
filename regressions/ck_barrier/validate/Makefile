.PHONY: clean distribution

OBJECTS=validate combining_validate 

all: $(OBJECTS)

validate: validate.c ../../../include/ck_barrier.h
	$(CC) $(CFLAGS) -o validate validate.c -lpthread

combining_validate: combining_validate.c ../../../include/ck_barrier.h
	$(CC) $(CFLAGS) -o combining_validate combining_validate.c -lpthread

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -O0 -ggdb

.PHONY: clean distribution

OBJECTS=validate 

all: $(OBJECTS)

validate: validate.c
	$(CC) $(CFLAGS) -o validate validate.c -lpthread

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

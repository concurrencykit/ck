.PHONY: clean distribution

OBJECTS=serial validate 

all: $(OBJECTS)

validate: validate.c
	$(CC) $(CFLAGS) -o validate validate.c -lpthread

serial: serial.c
	$(CC) $(CFLAGS) -o serial serial.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-ggdb -D_GNU_SOURCE

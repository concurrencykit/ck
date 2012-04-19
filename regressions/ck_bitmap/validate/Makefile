.PHONY: clean

OBJECTS=serial

all: $(OBJECTS)

serial: serial.c
	$(CC) $(CFLAGS) -o serial serial.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

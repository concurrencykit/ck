.PHONY: clean distribution

OBJECTS=ck_hp.o

all: libck.so libck.a

libck.so: $(OBJECTS)
	$(CC) $(CFLAGS) -shared -fPIC -o libck.so $(OBJECTS)

libck.a: $(OBJECTS)
	ar rcs libck.a $(OBJECTS)

ck_hp.o: ck_hp.c
	$(CC) $(CFLAGS) -c -o ck_hp.o ck_hp.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS) libck.a libck.so

include ../build/ck.build
CFLAGS+=-fPIC

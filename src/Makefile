.PHONY: clean distribution

OBJECTS=ck_hp.o				\
	ck_barrier_centralized.o	\
	ck_barrier_combining.o		\
	ck_barrier_dissemination.o	\
	ck_barrier_tournament.o		\
	ck_barrier_mcs.o

all: libck.so libck.a

libck.so: $(OBJECTS)
	$(LD) $(LDFLAGS) -o libck.so $(OBJECTS)

libck.a: $(OBJECTS)
	ar rcs libck.a $(OBJECTS)

ck_hp.o: ck_hp.c
	$(CC) $(CFLAGS) -c -o ck_hp.o ck_hp.c

ck_barrier_centralized.o: ck_barrier_centralized.c
	$(CC) $(CFLAGS) -c -o ck_barrier_centralized.o ck_barrier_centralized.c

ck_barrier_combining.o: ck_barrier_combining.c
	$(CC) $(CFLAGS) -c -o ck_barrier_combining.o ck_barrier_combining.c

ck_barrier_dissemination.o: ck_barrier_dissemination.c
	$(CC) $(CFLAGS) -c -o ck_barrier_dissemination.o ck_barrier_dissemination.c

ck_barrier_tournament.o: ck_barrier_tournament.c
	$(CC) $(CFLAGS) -c -o ck_barrier_tournament.o ck_barrier_tournament.c

ck_barrier_mcs.o: ck_barrier_mcs.c
	$(CC) $(CFLAGS) -c -o ck_barrier_mcs.o ck_barrier_mcs.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS) libck.a libck.so

include ../build/ck.build
CFLAGS+=-fPIC

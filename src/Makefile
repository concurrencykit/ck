.PHONY: clean distribution

OBJECTS=ck_barrier_centralized.o	\
	ck_barrier_combining.o		\
	ck_barrier_dissemination.o	\
	ck_barrier_tournament.o		\
	ck_barrier_mcs.o		\
	ck_epoch.o			\
	ck_ht.o				\
	ck_hp.o				\
	ck_bag.o			\
	ck_hs.o

all: libck.so libck.a

libck.so: $(OBJECTS)
	$(LD) $(LDFLAGS) -o libck.so $(OBJECTS)

libck.a: $(OBJECTS)
	ar rcs libck.a $(OBJECTS)

ck_bag.o: ../include/ck_bag.h ck_bag.c
	$(CC) $(CFLAGS) -c -o ck_bag.o ck_bag.c

ck_epoch.o: ../include/ck_epoch.h ck_epoch.c
	$(CC) $(CFLAGS) -c -o ck_epoch.o ck_epoch.c

ck_hs.o: ../include/ck_hs.h ck_hs.c
	$(CC) $(CFLAGS) -c -o ck_hs.o ck_hs.c

ck_ht.o: ../include/ck_ht.h ck_ht.c
	$(CC) $(CFLAGS) -c -o ck_ht.o ck_ht.c

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

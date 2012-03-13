.PHONY: clean distribution

HEADER=../../../include/ck_queue.h
OBJECTS=ck_slist ck_list

all: $(OBJECTS)

ck_slist: $(HEADER) ck_slist.c
	$(CC) $(CFLAGS) -o ck_slist ck_slist.c

ck_list: $(HEADER) ck_list.c
	$(CC) $(CFLAGS) -o ck_list ck_list.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE

.PHONY: all clean

GZIP=gzip
OBJECTS=ck_ht_allocator_set.3.gz  \
	ck_ht_count.3.gz 	  \
	ck_ht_destroy.3.gz	  \
	ck_ht_get_spmc.3.gz	  \
	ck_ht_grow_spmc.3.gz	  \
	ck_ht_hash.3.gz	  	  \
	ck_ht_hash_direct.3.gz	  \
	ck_ht_init.3.gz	  	  \
	ck_ht_put_spmc.3.gz	  \
	ck_ht_remove_spmc.3.gz	  \
	ck_ht_reset_spmc.3.gz	  \
	ck_ht_set_spmc.3.gz

all: $(OBJECTS)

ck_ht_allocator_set.3.gz: ck_ht_allocator_set.3
	$(GZIP) -c ck_ht_allocator_set.3 > ck_ht_allocator_set.3.gz

ck_ht_count.3.gz: ck_ht_count.3
	$(GZIP) -c ck_ht_count.3 > ck_ht_count.3.gz

ck_ht_destroy.3.gz: ck_ht_destroy.3
	$(GZIP) -c ck_ht_destroy.3 > ck_ht_destroy.3.gz

ck_ht_get_spmc.3.gz: ck_ht_get_spmc.3
	$(GZIP) -c ck_ht_get_spmc.3 > ck_ht_get_spmc.3.gz

ck_ht_grow_spmc.3.gz: ck_ht_grow_spmc.3
	$(GZIP) -c ck_ht_grow_spmc.3 > ck_ht_grow_spmc.3.gz

ck_ht_hash.3.gz: ck_ht_hash.3
	$(GZIP) -c ck_ht_hash.3 > ck_ht_hash.3.gz

ck_ht_hash_direct.3.gz: ck_ht_hash_direct.3
	$(GZIP) -c ck_ht_hash_direct.3 > ck_ht_hash_direct.3.gz

ck_ht_init.3.gz: ck_ht_init.3
	$(GZIP) -c ck_ht_init.3 > ck_ht_init.3.gz

ck_ht_put_spmc.3.gz: ck_ht_put_spmc.3
	$(GZIP) -c ck_ht_put_spmc.3 > ck_ht_put_spmc.3.gz

ck_ht_remove_spmc.3.gz: ck_ht_remove_spmc.3
	$(GZIP) -c ck_ht_remove_spmc.3 > ck_ht_remove_spmc.3.gz

ck_ht_reset_spmc.3.gz: ck_ht_reset_spmc.3
	$(GZIP) -c ck_ht_reset_spmc.3 > ck_ht_reset_spmc.3.gz

ck_ht_set_spmc.3.gz: ck_ht_set_spmc.3
	$(GZIP) -c ck_ht_set_spmc.3 > ck_ht_set_spmc.3.gz

clean:
	rm -f *~ *.o *.3.gz


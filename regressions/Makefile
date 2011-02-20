.PHONY: all clean

all:
	make -C ./ck_spinlock/validate all
	make -C ./ck_spinlock/benchmark all
	make -C ./ck_fifo/validate all
	make -C ./ck_pr/validate all 
	make -C ./ck_pr/benchmark all
	make -C ./ck_bytelock/validate all
	make -C ./ck_bytelock/benchmark all
	make -C ./ck_sequence/validate all 
	make -C ./ck_stack/validate all
	make -C ./ck_ring/validate all
	make -C ./ck_hp/validate all

clean:
	make -C ./ck_spinlock/validate clean
	make -C ./ck_spinlock/benchmark clean
	make -C ./ck_fifo/validate clean
	make -C ./ck_pr/validate clean 
	make -C ./ck_pr/benchmark clean
	make -C ./ck_bytelock/validate clean
	make -C ./ck_bytelock/benchmark clean 
	make -C ./ck_sequence/validate clean
	make -C ./ck_stack/validate clean
	make -C ./ck_ring/validate clean
	make -C ./ck_hp/validate clean 


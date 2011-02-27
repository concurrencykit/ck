.PHONY: all clean

all:
	make -C ./ck_spinlock/validate all
	make -C ./ck_spinlock/benchmark all
	make -C ./ck_fifo/validate all
	make -C ./ck_fifo/benchmark all
	make -C ./ck_pr/validate all 
	make -C ./ck_pr/benchmark all
	make -C ./ck_barrier/validate all
	make -C ./ck_barrier/benchmark all
	make -C ./ck_bytelock/validate all
	make -C ./ck_bytelock/benchmark all
	make -C ./ck_sequence/validate all 
	make -C ./ck_stack/validate all
	make -C ./ck_stack/benchmark all
	make -C ./ck_ring/validate all
	make -C ./ck_hp/validate all
	make -C ./ck_hp/benchmark all

clean:
	make -C ./ck_spinlock/validate clean
	make -C ./ck_spinlock/benchmark clean
	make -C ./ck_fifo/validate clean
	make -C ./ck_fifo/benchmark clean 
	make -C ./ck_pr/validate clean 
	make -C ./ck_pr/benchmark clean
	make -C ./ck_barrier/validate clean 
	make -C ./ck_barrier/benchmark clean 
	make -C ./ck_bytelock/validate clean
	make -C ./ck_bytelock/benchmark clean 
	make -C ./ck_sequence/validate clean
	make -C ./ck_stack/validate clean
	make -C ./ck_stack/benchmark clean 
	make -C ./ck_ring/validate clean
	make -C ./ck_hp/validate clean 
	make -C ./ck_hp/benchmark clean 


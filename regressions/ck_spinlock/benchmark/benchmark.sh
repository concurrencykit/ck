#!/bin/sh

SPACE="      "
SYSTEM=`uname -s`

case "$SYSTEM" in
	"FreeBSD")
	CORES=`sysctl -n hw.ncpu`
	;;
	"Darwin")
	CORES=`sysctl -n hw.activecpu`
	;;
	"Linux")
	CORES=`grep processor /proc/cpuinfo|wc -l` 
	;;
esac

echo "Detected $CORES cores."
echo

for k in ck_clh ck_anderson ck_cas ck_dec ck_fas ck_mcs ck_ticket ck_ticket_pb; do
	echo "===[ Beginning $k benchmarks..."
	echo "# Cores $SPACE           Total $SPACE   Average $SPACE Deviation" > ${k}.data

	for j in `seq 1 $CORES`; do
		printf "     Beginning $j cores..."
		printf "  $j $SPACE    " >> ${k}.data;
		./$k $j 1 0 2> /dev/null | awk '/deviation/ {printf("%16f ",$4)} /average/ {printf("%16.4d ",$4)} /total/ {printf("%16d ",$4)}' >> ${k}.data
		echo >> ${k}.data
		printf "done\n"
	done
done

#!/bin/sh

SPACE="      "
SYSTEM=`uname -s`

case "$SYSTEM" in
	"Darwin")
	CORES=`sysctl -n hw.activecpu`
	;;
	"Linux")
	CORES=`grep processor /proc/cpuinfo|wc -l` 
	;;
esac

echo "Detected $CORES cores."
echo

for k in push pair pop; do
	echo "===[ Beginning $k benchmarks..."
	printf "# Cores $SPACE" > ${k}.data

	for i in *_${k}; do
		printf "$i $SPACE" >> ${k}.data
	done

	echo >> ${k}.data

	for j in `seq 1 $CORES`; do
		echo "===[ Beginning $j cores..."

		printf "  $j $SPACE    " >> ${k}.data;
		for i in *_${k}; do
			printf "     Executing $i..."
			./$i $j 1 0 | awk '{printf("%.8f      ",$2)}' >> ${k}.data
			printf "done\n"
		done
		echo >> ${k}.data
	done
done

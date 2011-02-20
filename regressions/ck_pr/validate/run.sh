#!/bin/sh

OPERATIONS="pr_add pr_and pr_btc pr_btr pr_bts pr_cas pr_dec pr_faa pr_fas pr_inc pr_load pr_or pr_store pr_sub pr_xor"

COMPILE=0
PASSED=0
TOTAL=0

make > /dev/null || exit 1

for i in $OPERATIONS; do
	echo "ck_${i} --------------------"
	./ck_${i}
	if test $? -eq 0; then
		PASSED=`expr $PASSED + 1`
	fi

	TOTAL=`expr $TOTAL + 1`
done

echo "=================================="
echo "Passed: $PASSED/$TOTAL"
echo "=================================="

if test $PASSED -eq $TOTAL; then
	EXIT=0
else
	EXIT=1
fi

exit $EXIT

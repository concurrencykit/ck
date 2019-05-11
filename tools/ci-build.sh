#!/bin/sh
#
# Skeleton for continuous integration testing.
##############################################################################

set -x
./configure $@
if [ `uname -s` = "FreeBSD" ]; then
	make -j $(sysctl -n hw.ncpu)
else
	make -j
fi

#!/bin/sh
#
# Skeleton for continuous integration testing.
##############################################################################

set -x

export CFLAGS="-DITERATE=400 -DPAIRS_S=100 -DITERATIONS=24"
./configure $@

if [ `uname -s` = "FreeBSD" ]; then
	make -j $(sysctl -n hw.ncpu)
else
	make -j
fi

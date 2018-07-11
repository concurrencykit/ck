#!/bin/sh
#
# Skeleton for continuous integration testing.
##############################################################################

set -x
./configure $@
make -j


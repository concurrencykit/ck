#!/bin/sh
#
# Skeleton for Travis integration testing.
##############################################################################

set -x
./configure
make -j


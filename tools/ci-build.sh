#!/bin/sh
#
# Skeleton for continuous integration testing.
##############################################################################

set -x

# Determine the number of processor cores available but favor a value specified
# as a command line argument to the script, if provided
CORES=
for arg do
    # Look for `--cores=n` and set `CORES=n` if found
    if [ `echo "${arg}" | cut -d'=' -f1` = "--cores" ]; then
        CORES=`echo "${arg}" | cut -d'=' -f2`
    fi
done
ARGS="${@}"
if [ -z "${CORES}" ]; then
    # `--cores` not provided, attempt to determine it using appropriate utilities
    if command -v nproc; then
        # Linux and FreeBSD 13.2+
        CORES=`nproc`
    elif command -v sysctl; then
        # macOS and BSDs
        CORES=`sysctl -n hw.ncpu`
    fi
    if [ ! -z "${CORES}" ]; then
        ARGS="${ARGS} --cores=${CORES}"
    fi
fi

export CFLAGS="-DITERATE=400 -DPAIRS_S=100 -DITERATIONS=24 -DSTEPS=10000"
./configure ${ARGS}

make -j${CORES}

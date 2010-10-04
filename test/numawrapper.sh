#!/bin/bash
nodecount=$(numactl --hardware | awk '/^available:/{print $2}')
#if [[ $PORTALS4_NUM_PROCS -lt $nodecount ]] ; then
#	arg="--cpunodebind=$PORTALS4_RANK"
#else
	# for Hyperthreads, assign every-other to get true cores
	base=$(($PORTALS4_RANK*6))
	arg="--physcpubind="$base,$(($base+2)),$(($base+4))
#fi
exec numactl $arg "$@"

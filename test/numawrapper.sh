#!/bin/bash
#nodecount=$(numactl --hardware | awk '/^available:/{print $2}')
case "$1" in
	--socket-dyn)
	shift
	arg="--cpunodebind=$PORTALS4_RANK"
	;;
	--socket-static)
	shift
	case "$PORTALS4_RANK" in
		0)
		base=2
		;;
		1)
		base=3
		;;
	esac
	arg="--physcpubind="$base,$(($base+2))
	;;
	--core-static)
	shift
	case "$PORTALS4_RANK" in
		0)
		base=4
		;;
		1)
		base=8
		;;
	esac
	arg="--physcpubind="$base,$(($base+2))
	;;
	--core-dyn)
	shift
	arg="--cpunodebind=0"
	;;
	*)
	echo "Valid flags: --socket-dyn    (socket-to-socket, dynamic core assignment)"
	echo "             --socket-static (socket-to-socket, static core assignment)"
	echo "             --core-dyn      (core-to-core, dynamic core assignment)"
	echo "             --core-static   (core-to-core, static core assignment)"
	exit
	;;
esac
echo RANK $PORTALS4_RANK: $arg
exec numactl $arg "$@"

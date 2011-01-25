#!/bin/bash
hardwareoutput="$(numactl --hardware)"
nodecount=$(awk '/^available:/{print $2}' <<<"$hardwareoutput")
node0nodes=( $(grep "node 0 cpus:" <<<"$hardwareoutput" | cut -d: -f2) )
node1nodes=( $(grep "node 1 cpus:" <<<"$hardwareoutput" | cut -d: -f2) )
node0cores=${#node0nodes[*]}
node1cores=${#node1nodes[*]}
verbose="no"
if [ "$1" == "--verbose" ] ; then
	shift
	verbose="yes"
fi
case "$1" in
	--socket-dyn*)
	shift
	arg="--cpunodebind="$((${PORTALS4_RANK}%${nodecount}))
	;;
	--socket-static2)
	shift
	if [ "$PORTALS4_NUM_PROCS" -le 6 ] ; then
		case "$PORTALS4_RANK" in
		4) base=0 ;; # 0 + 2  [socket 0]
		5) base=1 ;; # 1 + 3  [socket 1]
		0) base=4 ;; # 4 + 6  [socket 0]
		2) base=5 ;; # 5 + 7  [socket 1]
		1) base=8 ;; # 8 + 10 [socket 0]
		3) base=9 ;; # 9 + 11 [socket 1]
		esac
	    arg="--physcpubind="$base,$(($base+2))
	else
	    totcores=$(($node0cores+$node1cores))
	    idx=$(($PORTALS4_RANK%$totcores))
	    if [ $idx -ge $node0cores ] ; then
		base=${node1nodes[$(($idx-$node0cores))]}
	    else
		base=${node0nodes[$idx]}
	    fi
	    arg="--physcpubind=$base"
	fi
	;;
	--socket-static)
	shift
	if [ "$PORTALS4_NUM_PROCS" -le 6 ] ; then
		case "$PORTALS4_RANK" in
		0) base=0 ;; # 0 + 2  [socket 0]
		1) base=1 ;; # 1 + 3  [socket 1]
		2) base=4 ;; # 4 + 6  [socket 0]
		3) base=5 ;; # 5 + 7  [socket 1]
		4) base=8 ;; # 8 + 10 [socket 0]
		5) base=9 ;; # 9 + 11 [socket 1]
		esac
	    arg="--physcpubind="$base,$(($base+2))
	else
	    totcores=$(($node0cores+$node1cores))
	    idx=$(($PORTALS4_RANK%$totcores))
	    if [ $idx -ge $node0cores ] ; then
			base=${node1nodes[$(($idx-$node0cores))]}
	    else
			base=${node0nodes[$idx]}
	    fi
	    arg="--physcpubind=$base"
	fi
	;;
	--core-static)
	shift
	if [ "$PORTALS4_NUM_PROCS" == 2 ] ; then
	    case "$PORTALS4_RANK" in
		0)
		    base=4
		    ;;
		1)
		    base=8
		    ;;
	    esac
	    arg="--physcpubind="$base,$(($base+2))
	elif [ "$PORTALS4_NUM_PROCS" -lt $(($node1cores/2)) ] ; then
		base=${node1nodes[$(($PORTALS4_RANK%${#node1nodes[*]}))]}
		arg="--physcpubind=$base,"$(($base+2))
	elif [ "$PORTALS4_NUM_PROCS" -le $node1cores ] ; then
	    base=${node1nodes[$(($PORTALS4_RANK%${#node1nodes[*]}))]}
	    arg="--physcpubind=$base"
	else
	    echo "Too many ranks! Max for core-static is $node1cores"
	    exit
	fi
	;;
	--core-dyn*)
	shift
	arg="--cpunodebind=1"
	;;
	*)
	echo "Valid flags: --socket-dyn    (socket-to-socket, dynamic core assignment)"
	echo "             --socket-static (socket-to-socket, static core assignment)"
	echo "             --core-dyn      (core-to-core, dynamic core assignment)"
	echo "             --core-static   (core-to-core, static core assignment)"
	exit
	;;
esac
[ $verbose == "yes" ] && echo RANK $PORTALS4_RANK: $arg
exec numactl $arg "$@"

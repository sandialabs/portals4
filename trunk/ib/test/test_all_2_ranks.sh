#!/bin/bash

if [ x"$OMPI_COMM_WORLD_RANK" = x ] ; then
	echo "Must run with mpirun. For instance:"
	echo "   mpirun  -H localhost -n 2 ./test_all_2_ranks"
	exit 1
fi

echo "===== test_basic_init.xml ====="
./ptl_test -f test_basic_init.xml || exit 1

echo "===== test_basic_ni.xml ====="
./ptl_test -f test_basic_ni.xml || exit 1

echo "===== test_basic_handle.xml ====="
./ptl_test -f test_basic_handle.xml || exit 1

echo "===== test_basic_pt.xml ====="
./ptl_test -f test_basic_pt.xml || exit 1

echo "===== test_basic_eq.xml ====="
./ptl_test -f test_basic_eq.xml || exit 1

echo "===== test_basic_ct.xml ====="
./ptl_test -f test_basic_ct.xml || exit 1

echo "===== test_basic_id.xml ====="
./ptl_test -f test_basic_id.xml || exit 1

echo "===== test_basic_md.xml ====="
./ptl_test -f test_basic_md.xml || exit 1

echo "===== test_basic_le.xml ====="
./ptl_test -f test_basic_le.xml || exit 1

echo "===== test_basic_me.xml ====="
./ptl_test -f test_basic_me.xml || exit 1

echo "===== test_basic_move.xml ====="
./ptl_test -f test_basic_move.xml || exit 1

echo "===== test_rdma_move_logical.xml ====="
./ptl_test -f test_rdma_move_logical.xml || exit 1

echo "===== test_basic_trig.xml ====="
./ptl_test -f test_basic_trig.xml || exit 1

echo "===== test_limits.xml ====="
./ptl_test -f test_limits.xml || exit 1

echo "===== test_events_logical.xml ====="
./ptl_test -f test_events_logical.xml || exit 1

echo "===== test_ct_events.xml ====="
./ptl_test -f test_ct_events.xml || exit 1

echo "===== test_flow.xml ===="
./ptl_test -f test_flow.xml || exit 1

echo "===== test_eq_overflow.xml ===="
./ptl_test -f test_eq_overflow.xml || exit 1

echo "===== test_unlink.xml ====="
./ptl_test -f test_unlink.xml || exit 1

echo "===== test_short.xml ====="
./ptl_test -f test_short.xml || exit 1

echo "===== test_atomic_logical_lb.xml ====="
./ptl_test -f test_atomic_logical_lb.xml || exit 1

echo "===== test_swap_logical_lb.xml ====="
./ptl_test -f test_swap_logical_lb.xml || exit 1

echo "===== test_put_overflow.xml ====="
./ptl_test -f test_put_overflow.xml || exit 1

echo "===== test_atomic_overflow.xml ====="
./ptl_test -f test_atomic_overflow.xml || exit 1

echo "===== test_search.xml ====="
./ptl_test -f test_search.xml || exit 1

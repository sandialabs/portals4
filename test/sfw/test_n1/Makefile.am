TESTS = $(top_srcdir)/test/sfw/test_n1/*.xml

EXTRA_DIST = $(DESTS)

NPROCS ?= 1 ../ptl_test -f
TESTS_ENVIRONMENT = $(top_builddir)/src/runtime/hydra/yod.hydra -np $(NPROCS)

# vim:ft=automake
BENCHMARKS += P4msgrate

P4msgrate_SOURCES = \
    benchmarks/msg_rate/test_one_way.h               \
    benchmarks/msg_rate/test_prepost.h               \
    benchmarks/msg_rate/P4msgrate.c                  \
    benchmarks/msg_rate/test_one_wayME.c             \
    benchmarks/msg_rate/test_one_wayLE.c             \
    benchmarks/msg_rate/test_prepostME.c             \
    benchmarks/msg_rate/test_prepostLE.c

P4msgrate_DEPENDENCIES = $(DEPENDENCIES_TEST)
                
P4msgrate_CPPFLAGS = $(AM_CPPFLAGS) -Ibenchmarks/msg_rate

# vim:ft=automake
EXTRA_PROGRAMS += P4msgrate

P4msgrate_SOURCES = \
    msg_rate/test_one_way.h               \
    msg_rate/test_prepost.h               \
    msg_rate/P4msgrate.c                  \
    msg_rate/test_one_wayME.c             \
    msg_rate/test_one_wayLE.c             \
    msg_rate/test_prepostME.c             \
    msg_rate/test_prepostLE.c

P4msgrate_CPPFLAGS = $(AM_CPPFLAGS) -Imsg_rate

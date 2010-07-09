# vim:ft=make

AM_CPPFLAGS += -I$(top_srcdir)/src/shmem/include

libportals_la_SOURCES += \
						 shmem/init.c

yod_SOURCES = shmem/yod.c

# vim:ft=make

AM_CPPFLAGS += -I$(top_srcdir)/src/shmem/include

libportals_la_SOURCES += \
			 shmem/net_iface.c \
			 shmem/init.c

yod_SOURCES = shmem/yod.c

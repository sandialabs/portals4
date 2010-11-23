# vim:ft=automake

AM_CPPFLAGS += -I$(top_srcdir)/src/support/include

libP4support_la_SOURCES += \
			 support/libP4support.c

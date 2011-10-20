# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation


AM_CPPFLAGS = -I$(top_srcdir)/include

include_HEADERS = portals4_runtime.h

lib_LTLIBRARIES = libportals.la
bin_PROGRAMS = yod

# version-info fields are:
# 1. the current interface revision number (i.e. whenever arguments of existing
#    functions change, increment this number)
# 2. the revision number (the library with the highest revision number will be
#    preferred)
# 3. the age (how many previous interfaces are supported)
#
# So...
# - If you add functions, increase the interface number AND the age, and reset
#   the revision number to 0.
# - If you modify (or remove) existing functions, increase the interface number
#   and reset the revision and age numbers to 0
# - If you support all the same functions (just bugfixes or whatnot), increment
#   the revision number
libportals_la_LDFLAGS = -version-info 4:0:0 -no-undefined
libportals_la_SOURCES =

yod_SOURCES =

noinst_HEADERS =

AM_CPPFLAGS += -I$(top_srcdir)/src/shmem/include

libportals_la_SOURCES += \
			 net_iface.c \
			 event_queues.c \
			 counting_events.c \
			 memory_descriptors.c \
			 list_entries.c \
			 matching_list_entries.c \
			 process_identification.c \
			 table_entries.c \
			 data_movement.c \
			 fragments.c \
			 nemesis.c \
			 perform_atomic.c \
			 handle.c \
			 triggered.c \
			 bundling.c \
			 runtime.c \
			 uid.c \
			 init.c

if USE_TRANSFER_ENGINE
libportals_la_SOURCES += transfer_engine.c
endif

if USE_KNEM
libportals_la_SOURCES += knem.c
endif

if USE_PAPI
libportals_la_SOURCES += papi.c
endif

if USE_RDTSC
libportals_la_SOURCES += rdtsc.c
endif

yod_SOURCES += yod.c
yod_LDADD = libportals.la

noinst_HEADERS += \
		include/ptl_internal_CT.h \
		include/ptl_internal_DM.h \
		include/ptl_internal_EQ.h \
		include/ptl_internal_LE.h \
		include/ptl_internal_ME.h \
		include/ptl_internal_MD.h \
		include/ptl_internal_PT.h \
		include/ptl_internal_assert.h \
		include/ptl_internal_atomic.h \
		include/ptl_internal_commpad.h \
		include/ptl_internal_error.h \
		include/ptl_internal_fragments.h \
		include/ptl_internal_handles.h \
		include/ptl_internal_nemesis.h \
		include/ptl_internal_nit.h \
		include/ptl_internal_performatomic.h \
		include/ptl_internal_pid.h \
		include/ptl_internal_queues.h \
		include/ptl_internal_timer.h \
		include/ptl_internal_locks.h \
		include/ptl_visibility.h

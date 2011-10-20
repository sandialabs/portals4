# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation


AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/shmem/include

include_HEADERS = portals4_runtime.h
noinst_LTLIBRARIES = libportals_shmem.la

bin_PROGRAMS = yod
yod_SOURCES = yod.c
yod_LDADD = libportals_shmem.la

libportals_shmem_la_SOURCES = \
	include/ptl_internal_commpad.h \
	include/ptl_internal_ints.h \
	include/ptl_internal_alignment.h \
	include/ptl_internal_shm.h \
	include/ptl_internal_transfer_engine.h \
	include/ptl_internal_trigger.h \
	include/ptl_internal_knem.h \
	include/ptl_internal_papi.h \
	include/ptl_internal_orderednemesis.h \
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
	include/ptl_visibility.h \
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
libportals_shmem_la_SOURCES += transfer_engine.c
endif

if USE_KNEM
libportals_shmem_la_SOURCES += knem.c
endif

if USE_PAPI
libportals_shmem_la_SOURCES += papi.c
endif

if USE_RDTSC
libportals_shmem_la_SOURCES += rdtsc.c
endif


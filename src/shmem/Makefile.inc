# vim:ft=automake

AM_CPPFLAGS += -I$(top_srcdir)/src/shmem/include

libportals_la_SOURCES += \
			 shmem/net_iface.c \
			 shmem/event_queues.c \
			 shmem/counting_events.c \
			 shmem/memory_descriptors.c \
			 shmem/list_entries.c \
			 shmem/matching_list_entries.c \
			 shmem/process_identification.c \
			 shmem/table_entries.c \
			 shmem/data_movement.c \
			 shmem/fragments.c \
			 shmem/nemesis.c \
			 shmem/perform_atomic.c \
			 shmem/handle.c \
			 shmem/triggered.c \
			 shmem/bundling.c \
			 shmem/runtime.c \
			 shmem/uid.c \
			 shmem/jid.c \
			 shmem/init.c \
			 shmem/transfer_engine.c

if USE_KNEM
libportals_la_SOURCES += \
			  shmem/knem.c
endif

if USE_PAPI
libportals_la_SOURCES += \
			 shmem/papi.c
endif

if USE_RDTSC
libportals_la_SOURCES += \
			 shmem/rdtsc.c
endif

yod_SOURCES += shmem/yod.c
yod_LDADD = libportals.la

noinst_HEADERS += \
		shmem/include/ptl_internal_CT.h \
		shmem/include/ptl_internal_DM.h \
		shmem/include/ptl_internal_EQ.h \
		shmem/include/ptl_internal_LE.h \
		shmem/include/ptl_internal_ME.h \
		shmem/include/ptl_internal_MD.h \
		shmem/include/ptl_internal_PT.h \
		shmem/include/ptl_internal_assert.h \
		shmem/include/ptl_internal_atomic.h \
		shmem/include/ptl_internal_commpad.h \
		shmem/include/ptl_internal_error.h \
		shmem/include/ptl_internal_fragments.h \
		shmem/include/ptl_internal_handles.h \
		shmem/include/ptl_internal_nemesis.h \
		shmem/include/ptl_internal_nit.h \
		shmem/include/ptl_internal_performatomic.h \
		shmem/include/ptl_internal_pid.h \
		shmem/include/ptl_internal_queues.h \
		shmem/include/ptl_internal_timer.h \
		shmem/include/ptl_internal_locks.h \
		shmem/include/ptl_visibility.h

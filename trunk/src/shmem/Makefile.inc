# vim:ft=make

AM_CPPFLAGS += -I$(top_srcdir)/src/shmem/include

libportals_la_SOURCES += \
			 shmem/net_iface.c \
			 shmem/event_queues.c \
			 shmem/counting_events.c \
			 shmem/memory_descriptors.c \
			 shmem/list_entries.c \
			 shmem/matching_list_entries.c \
			 shmem/process_identification.c \
			 shmem/data_movement.c \
			 shmem/init.c

yod_SOURCES = shmem/yod.c

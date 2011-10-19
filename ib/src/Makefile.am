lib_LTLIBRARIES = libportals.la

libportals_la_CPPFLAGS = -I$(top_srcdir)/include
libportals_la_LIBADD = -libverbs -lpthread -lrt -lev
libportals_la_LDFLAGS = @LD_VERSION_SCRIPT@ -lrdmacm
libportals_la_SOURCES = \
	ptl.c \
	ptl_atomic.c \
	ptl_atomic.h \
	ptl_buf.c \
	ptl_buf.h \
	ptl_conn.c \
	ptl_conn.h \
	ptl_iface.c \
	ptl_iface.h \
	ptl_ct.c \
	ptl_ct.h \
	ptl_data.c \
	ptl_data.h \
	ptl_iov.c \
	ptl_eq.c \
	ptl_eq.h \
	ptl_evloop.c \
	ptl_evloop.h \
	ptl_gbl.c \
	ptl_gbl.h \
	ptl_handle.c \
	ptl_hdr.c \
	ptl_hdr.h \
	ptl_id.c \
	ptl_init.c \
	ptl_internal_alignment.h \
	ptl_internal_assert.h \
	ptl_internal_atomic.h \
	ptl_internal_locks.h \
	ptl_le.c \
	ptl_le.h \
	ptl_loc.h \
	ptl_log.h \
	ptl_md.c \
	ptl_md.h \
	ptl_me.c \
	ptl_me.h \
	ptl_move.c \
	ptl_mr.c \
	ptl_mr.h \
	ptl_ni.c \
	ptl_ni.h \
	ptl_obj.c \
	ptl_obj.h \
	ptl_param.c \
	ptl_param.h \
	ptl_pt.c \
	ptl_pt.h \
	ptl_rdma.c \
	ptl_recv.c \
	ptl_ref.h \
	ptl_send.c \
	ptl_tgt.c \
	ptl_types.h \
	ptl_xx.c \
	ptl_xx.h \
	tree.h

if WITH_TRANSPORT_SHMEM
libportals_la_SOURCES += \
	ptl_knem.c \
	ptl_nemesis.c \
	ptl_nemesis.h \
	ptl_shmem.c
endif

EXTRA_DIST = portals4.map

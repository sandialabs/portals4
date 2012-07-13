
AM_CPPFLAGS = -I$(top_srcdir)/include $(ev_CPPFLAGS) $(ofed_CPPFLAGS)
noinst_LTLIBRARIES = libportals_ib.la

if !WITH_PPE
libportals_ib_la_LIBADD = $(ev_LIBS) $(ofed_LIBS) -lpthread 
libportals_ib_la_LDFLAGS = $(ev_LDFLAGS) $(ofed_LDFLAGS)
libportals_ib_la_SOURCES = \
	ptl_atomic.c \
	ptl_atomic.h \
	ptl_buf.c \
	ptl_buf.h \
	ptl.c \
	ptl_conn.c \
	ptl_conn.h \
	ptl_ct.c \
	ptl_ct.h \
	ptl_ct_common.c \
	ptl_ct_common.h \
	ptl_data.c \
	ptl_data.h \
	ptl_eq.c \
	ptl_eq.h \
	ptl_eq_common.c \
	ptl_eq_common.h \
	ptl_evloop.c \
	ptl_evloop.h \
	ptl_gbl.h \
	ptl_hdr.h \
	ptl_id.c \
	ptl_iface.c \
	ptl_iface.h \
	ptl_init.c \
	ptl_iov.c \
	ptl_le.c \
	ptl_le.h \
	ptl_list.h \
	ptl_loc.h \
	ptl_lockfree.h \
	ptl_locks.h \
	ptl_log.h \
	ptl_md.c \
	ptl_md.h \
	ptl_me.c \
	ptl_me.h \
	ptl_misc.c \
	ptl_misc.h \
	ptl_move.c \
	ptl_mr.c \
	ptl_mr.h \
	ptl_ni.c \
	ptl_ni.h \
	ptl_obj.c \
	ptl_obj.h \
	ptl_param.c \
	ptl_param.h \
	ptl_pool.h \
	ptl_pt.c \
	ptl_pt.h \
	ptl_recv.c \
	ptl_ref.h \
	ptl_sync.h \
	ptl_tgt.c \
	tree.h

if WITH_TRANSPORT_IB
libportals_ib_la_SOURCES += \
	ptl_rdma.c \
	ptl_iface_ib.c \
	ummunotify.h
endif

if WITH_TRANSPORT_SHMEM
libportals_ib_la_SOURCES += \
	ptl_mem.c \
	ptl_queue.c \
	ptl_queue.h \
	ptl_shmem.c

if USE_KNEM
libportals_ib_la_SOURCES += \
	ptl_knem.c
endif
endif

else
# PPE - (implies no SHMEM; IB optional)

libportals_ib_la_CFLAGS = -DIS_LIGHT_LIB
libportals_ib_la_LIBADD = $(ev_LIBS) $(ofed_LIBS) -lxpmem -lpthread -lrt
libportals_ib_la_LDFLAGS = $(ev_LDFLAGS) $(ofed_LDFLAGS)
libportals_ib_la_SOURCES = \
	ptl_ct_common.c \
	ptl_ct_common.h \
	ptl_eq_common.c \
	ptl_eq_common.h \
	ptl_light_lib.c \
	ptl_misc.c \
	ptl_misc.h \
	ptl_obj.h \
	ptl_param.c \
	ptl_param.h \
	ptl_ppe.h \
	ptl_queue.c \
	ptl_queue.h \
	ptl_xpmem.h

bin_PROGRAMS = p4ppe
p4ppe_CFLAGS = $(AM_CFLAGS) -DIS_PPE
p4ppe_LDADD = -lxpmem
p4ppe_LDFLAGS = $(ev_LIBS) $(ofed_LIBS) $(ev_LDFLAGS) $(ofed_LDFLAGS)
p4ppe_SOURCES = \
	p4ppe.c \
	p4ppe.h \
	ptl_atomic.c \
	ptl_atomic.h \
	ptl_buf.c \
	ptl_buf.h \
	ptl_conn.c \
	ptl_conn.h \
	ptl_ct.c \
	ptl_ct.h \
	ptl_ct_common.c \
	ptl_ct_common.h \
	ptl_data.c \
	ptl_data.h \
	ptl_eq.c \
	ptl_eq.h \
	ptl_eq_common.c \
	ptl_eq_common.h \
	ptl_evloop.c \
	ptl_evloop.h \
	ptl_gbl.h \
	ptl_hdr.h \
	ptl_id.c \
	ptl_iface.c \
	ptl_iface.h \
	ptl_init.c \
	ptl_iov.c \
	ptl_le.c \
	ptl_le.h \
	ptl_list.h \
	ptl_loc.h \
	ptl_log.h \
	ptl_md.c \
	ptl_md.h \
	ptl_me.c \
	ptl_me.h \
	ptl_mem.c \
	ptl_misc.c \
	ptl_misc.h \
	ptl_move.c \
	ptl_mr.c \
	ptl_mr.h \
	ptl_ni.c \
	ptl_ni.h \
	ptl_obj.c \
	ptl_obj.h \
	ptl_param.c \
	ptl_param.h \
	ptl_pool.h \
	ptl_ppe.h \
	ptl_pt.c \
	ptl_pt.h \
	ptl_queue.c \
	ptl_queue.h \
	ptl_recv.c \
	ptl_ref.h \
	ptl_sync.h \
	ptl_tgt.c \
	ptl_xpmem.h \
	tree.h

if WITH_TRANSPORT_IB
p4ppe_SOURCES += \
	ptl_rdma.c \
	ptl_iface_ib.c \
	ummunotify.h
endif

endif

EXTRA_DIST = portals4.map

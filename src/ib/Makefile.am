EXTRA_DIST = portals4.map
noinst_LTLIBRARIES = libportals_ib.la

if !WITH_PPE
libportals_ib_la_CPPFLAGS = -I$(top_srcdir)/include $(ev_CPPFLAGS) $(ofed_CPPFLAGS)
libportals_ib_la_LIBADD = $(ev_LIBS) $(ofed_LIBS) -lpthread 
libportals_ib_la_LDFLAGS = $(ev_LDFLAGS) $(ofed_LDFLAGS)
libportals_ib_la_SOURCES = \
	ptl_atomic.c \
	ptl_atomic.h \
	ptl_buf.c \
	ptl_buf.h \
	ptl_byteorder.h \
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
	ptl_fat_lib.c \
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
	tree.h \
	ptl_timer.h

if WITH_TRANSPORT_IB
libportals_ib_la_SOURCES += \
	ptl_rdma.c \
	ptl_iface_ib.c \
	ummunotify.h
endif

if WITH_TRANSPORT_SHMEM
libportals_ib_la_SOURCES += \
	ptl_knem.h \
	ptl_mem.c \
	ptl_queue.c \
	ptl_queue.h \
	ptl_shmem.c

if USE_KNEM
libportals_ib_la_SOURCES += \
	ptl_knem.c
endif
endif

if WITH_TRANSPORT_UDP
libportals_ib_la_SOURCES += \
	ptl_iface_udp.c \
	ptl_udp.c
endif

else
# PPE - (implies no SHMEM; IB optional)

libportals_ib_la_CPPFLAGS = -DIS_LIGHT_LIB -I$(top_srcdir)/include $(ev_CPPFLAGS) $(ofed_CPPFLAGS) $(XPMEM_CPPFLAGS)
libportals_ib_la_LIBADD = $(ev_LIBS) $(ofed_LIBS) $(XPMEM_LIBS) -lpthread -lrt
libportals_ib_la_LDFLAGS = $(ev_LDFLAGS) $(ofed_LDFLAGS) $(XPMEM_LDFLAGS)
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
	ptl_ppe.c \
	ptl_queue.c \
	ptl_queue.h \
	ptl_xpmem.h

if !HAVE_KITTEN
bin_PROGRAMS = p4ppe
p4ppe_CPPFLAGS = -DIS_PPE -I$(top_srcdir)/include $(ev_CPPFLAGS) $(ofed_CPPFLAGS) $(XPMEM_CPPFLAGS)
p4ppe_LDADD = libportals_ppe.la
p4ppe_SOURCES = p4ppe_main.c
endif

lib_LTLIBRARIES = libportals_ppe.la
libportals_ppe_la_CPPFLAGS = -DIS_PPE -I$(top_srcdir)/include $(ev_CPPFLAGS) $(ofed_CPPFLAGS) $(XPMEM_CPPFLAGS)
libportals_ppe_la_LDFLAGS = -static $(ev_LDFLAGS) $(ofed_LDFLAGS) $(XPMEM_LDFLAGS)
libportals_ppe_la_LIBADD = $(ev_LIBS) $(ofed_LIBS) $(XPMEM_LIBS)
libportals_ppe_la_SOURCES = \
	p4ppe.c \
	p4ppe.h \
	ptl_atomic.c \
	ptl_atomic.h \
	ptl_buf.c \
	ptl_buf.h \
	ptl_byteorder.h \
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
	ptl_ppe.c \
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

portals4_HEADERS = p4ppe.h
portals4dir = $(includedir)/portals4

if WITH_TRANSPORT_IB
libportals_ppe_la_SOURCES += \
	ptl_rdma.c \
	ptl_iface_ib.c \
	ummunotify.h
endif

if WITH_TRANSPORT_UDP
libportals_ppe_la_SOURCES += \
	ptl_iface_udp.c \
	ptl_udp.c
endif

endif

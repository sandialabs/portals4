lib_LTLIBRARIES = libportals.la
libportals_la_CPPFLAGS = -Wall -Werror -g -O0 -I$(top_srcdir)/include -DPTL_CHECK_POINTER -DPTL_LOG_ENABLE=4
libportals_la_LIBADD = -libverbs -lpthread -lrt
libportals_la_SOURCES = \
	ptl.c \
	ptl_ct.c \
	ptl_eq.c \
	ptl_gbl.c \
	ptl_handle.c \
	ptl_id.c \
	ptl_le.c \
	ptl_maps.c \
	ptl_mr.c \
	ptl_md.c \
	ptl_me.c \
	ptl_move.c \
	ptl_ni.c \
	ptl_obj.c \
	ptl_index.c \
	ptl_pt.c \
	ptl_ref.c \
	ptl_rpc.c \
	ptl_xx.c \
	ptl_buf.c \
	ptl_hdr.c \
	ptl_send.c \
	ptl_rdma.c \
	ptl_recv.c \
	ptl_init.c \
	ptl_data.c \
	ptl_atomic.c \
	ptl_tgt.c

bin_PROGRAMS = control
control_CPPFLAGS = -Wall -Werror -g -O0 -I$(top_srcdir)/include -DPTL_LOG_ENABLE=4
control_LDADD = -libverbs -lrdmacm -lpthread
control_SOURCES =  \
	ctl_main.c \
	ctl_config.c \
	ctl_ib.c \
	ptl_rpc.c

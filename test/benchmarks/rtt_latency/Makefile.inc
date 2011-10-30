# vim:ft=automake
BENCHMARKS += \
	CT_LE_rtt_latency \
	CT_ME_rtt_latency \
	EQ_LE_rtt_latency \
	EQ_ME_rtt_latency

CT_LE_rtt_latency_SOURCES = benchmarks/rtt_latency/ct_hotpotato.c
CT_LE_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
CT_LE_rtt_latency_DEPENDENCIES = $(DEPENDENCIES_TEST)

CT_ME_rtt_latency_SOURCES = benchmarks/rtt_latency/ct_hotpotato.c
CT_ME_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
CT_ME_rtt_latency_DEPENDENCIES = $(DEPENDENCIES_TEST)

EQ_LE_rtt_latency_SOURCES = benchmarks/rtt_latency/events_hotpotato.c
EQ_LE_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
EQ_LE_rtt_latency_DEPENDENCIES = $(DEPENDENCIES_TEST)

EQ_ME_rtt_latency_SOURCES = benchmarks/rtt_latency/events_hotpotato.c
EQ_ME_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
EQ_ME_rtt_latency_DEPENDENCIES = $(DEPENDENCIES_TEST)

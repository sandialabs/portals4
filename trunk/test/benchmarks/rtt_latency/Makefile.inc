# vim:ft=automake
BENCHMARKS += \
	CT_LE_rtt_latency \
	CT_ME_rtt_latency \
	EQ_LE_rtt_latency \
	EQ_ME_rtt_latency

RTT_LATENCY_DIR = benchmarks/rtt_latency

CT_LE_rtt_latency_SOURCES = $(RTT_LATENCY_DIR)/ct_hotpotato.c
CT_LE_rtt_latency_CPPFLAGS = -DINTERFACE=0
CT_LE_rtt_latency_DEPENDENCIES = $(PORTALSLIB)

CT_ME_rtt_latency_SOURCES = $(RTT_LATENCY_DIR)/ct_hotpotato.c
CT_ME_rtt_latency_CPPFLAGS = -DINTERFACE=1
CT_ME_rtt_latency_DEPENDENCIES = $(PORTALSLIB)

EQ_LE_rtt_latency_SOURCES = $(RTT_LATENCY_DIR)/events_hotpotato.c
EQ_LE_rtt_latency_CPPFLAGS = -DINTERFACE=0
EQ_LE_rtt_latency_DEPENDENCIES = $(PORTALSLIB)

EQ_ME_rtt_latency_SOURCES = $(RTT_LATENCY_DIR)/events_hotpotato.c
EQ_ME_rtt_latency_CPPFLAGS = -DINTERFACE=1
EQ_ME_rtt_latency_DEPENDENCIES = $(PORTALSLIB)

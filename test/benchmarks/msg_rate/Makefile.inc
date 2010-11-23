# vim:ft=automake
BENCHMARKS += \
	P4msgrate

MSG_RATE_DIR = benchmarks/msg_rate

P4msgrate_SOURCES = $(MSG_RATE_DIR)/P4msgrate.c
P4msgrate_LDADD = $(PORTALSLIB) $(P4SUPPORTLIB)

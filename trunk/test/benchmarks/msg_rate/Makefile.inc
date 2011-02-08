# vim:ft=automake
BENCHMARKS += \
	P4msgrate

MSG_RATE_DIR = benchmarks/msg_rate

P4msgrate_SOURCES = $(MSG_RATE_DIR)/P4msgrate.c \
		$(MSG_RATE_DIR)/test_one_wayME.c \
		$(MSG_RATE_DIR)/test_one_wayLE.c \
		$(MSG_RATE_DIR)/test_prepostME.c \
		$(MSG_RATE_DIR)/test_prepostLE.c
P4msgrate_CPPFLAGS = -I$(MSG_RATE_DIR)
P4msgrate_LDADD = $(PORTALSLIB) $(P4SUPPORTLIB)

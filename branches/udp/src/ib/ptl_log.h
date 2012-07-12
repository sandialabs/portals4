/*
 * ptl_log.h - logging and trace macros
 */

#ifndef PTL_LOG_H
#define PTL_LOG_H

#ifdef PTL_LOG_ENABLE

#define ptl_enter(format, arg...)					\
	do {								\
		if (ptl_log_level > 3)					\
		fprintf(stderr, "enter %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#define ptl_exit(format, arg...)					\
	do {								\
		if (ptl_log_level > 3)					\
		fprintf(stderr, "exit  %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#else

#define ptl_enter(format, arg...)
#define ptl_enter(format, arg...)

#endif

#define ptl_info(format, arg...)					\
	do {								\
		if (ptl_log_level > 2)					\
		fprintf(stderr, "info  %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#define ptl_warn(format, arg...)					\
	do {								\
		if (ptl_log_level > 1)					\
		fprintf(stderr, "warn  %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#define ptl_error(format, arg...)					\
	do {								\
		fprintf(stderr, "error %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#define ptl_fatal(format, arg...)					\
	do {								\
		fprintf(stderr, "fatal %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
		abort();						\
	} while (0)

extern int debug;

#define WARN()	do { if (debug) printf("\033[1;33mwarn:\033[0m %s(%s:%d)\n", __func__, __FILE__, __LINE__); } while(0)

#endif /* PTL_LOG_H */

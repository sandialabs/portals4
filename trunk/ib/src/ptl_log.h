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

#define ptl_info(format, arg...)					\
	do {								\
		if (ptl_log_level > 2)					\
		fprintf(stderr, "info  %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#else

#define ptl_enter(format, arg...)
#define ptl_enter(format, arg...)
#define ptl_info(format, arg...)

#endif

#define ptl_warn(format, arg...)					\
	do {								\
		if (ptl_log_level > 1)					\
		fprintf(stderr, "warn  %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#define ptl_error(format, arg...)					\
	do {								\
		if (ptl_log_level > 0)					\
		fprintf(stderr, "error %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
	} while (0)

#define ptl_fatal(format, arg...)					\
	do {								\
		if (ptl_log_level > 0)					\
		fprintf(stderr, "fatal %s(%s:%d): " format,		\
			__func__, __FILE__, __LINE__,  ## arg);		\
		abort();						\
	} while (0)

#endif /* PTL_LOG_H */

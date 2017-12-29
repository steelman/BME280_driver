#ifndef _FAULTD_LOG_H
#define _FAULTD_LOG_H

#include <stdlib.h>
#include <syslog.h>

#define log_full_errno(level, error, ...) ({							\
	int _l = (level), _e = error;                                       \
	(LOG_PRI(_l) <= log_get_max_level()) ?								\
	log_internal(_l, _e, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)	\
	: -abs(_e);															\
})

#define log_full(level, ...) log_full_errno(level, 0, __VA_ARGS__)

#define log_debug(...)                log_full(LOG_DEBUG,   __VA_ARGS__)
#define log_info(...)                 log_full(LOG_INFO,    __VA_ARGS__)
#define log_notice(...)               log_full(LOG_NOTICE,  __VA_ARGS__)
#define log_warning(...)              log_full(LOG_WARNING, __VA_ARGS__)
#define log_error(...)                log_full(LOG_ERR,     __VA_ARGS__)

#define log_debug_errno(error, ...)   log_full_errno(LOG_DEBUG,   error, __VA_ARGS__)
#define log_info_errno(error, ...)    log_full_errno(LOG_INFO,    error, __VA_ARGS__)
#define log_notice_errno(error, ...)  log_full_errno(LOG_NOTICE,  error, __VA_ARGS__)
#define log_warning_errno(error, ...) log_full_errno(LOG_WARNING, error, __VA_ARGS__)
#define log_error_errno(error, ...)   log_full_errno(LOG_ERR,     error, __VA_ARGS__)

int log_get_max_level(void);
void log_set_max_level(int level);
int log_internal(int level, int error, const char *file, int __line, const char *func, const char *format, ...) __attribute__ ((format (printf, 6, 7)));
int log_parse_level_name(const char *name);
int log_kmsg(const char *format, ...);
#endif  /* _FAUTLD_LOG_H */

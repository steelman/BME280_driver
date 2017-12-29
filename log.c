/*
 * This file is part of faultd.
 *
 * Copyright © 2017 Samsung Electronics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "log.h"

#define PROTECT_ERRNO								\
	__attribute__((cleanup(_reset_errno))) __attribute((unused))		\
		int _saved_errno = errno

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static int log_max_level = LOG_INFO;

static inline void _reset_errno(int *saved_errno)
{
	errno = *saved_errno;
}

int log_get_max_level()
{
	return log_max_level;
}

void log_set_max_level(int level)
{
	assert((level & LOG_PRIMASK) == level);
	log_max_level = level;
}

int log_parse_level_name(const char *name)
{
	size_t i;
	static const char *levels[] = {
		[LOG_EMERG] = "emerg",
		[LOG_ALERT] = "alert",
		[LOG_CRIT] = "crit",
		[LOG_ERR] = "err",
		[LOG_WARNING] = "warning",
		[LOG_NOTICE] = "notice",
		[LOG_INFO] = "info",
		[LOG_DEBUG] = "debug",
	};

	for (i = 0; i < ARRAY_SIZE(levels); i++)
		if (strcmp(levels[i], name) == 0)
			return i;
	return -EINVAL;
}

int log_internal(int level,
				 int error,
				 const char *file,
				 int line,
				 const char *func,
				 const char *format, ...)
{
	char buffer[LINE_MAX];
	va_list ap;
	PROTECT_ERRNO;

	if (error < 0)
		error = -error;

	if (LOG_PRI(level) > log_max_level)
		return -error;

	if (error != 0)
		errno = error;

	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	fprintf(stderr, "<%d>:%s:%d:%s:%s\n", level, file, line, func, buffer);
	fflush(stderr);
	return -error;
}

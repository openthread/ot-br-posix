/*
 *
 *    Copyright (c) 2010-2012 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    Description:
 *		This file implements a configuration file parser.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 1
#endif

#include "assert-macros.h"

#include "config-file.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <poll.h>
#include <sys/select.h>
#include <libgen.h>
#include <syslog.h>

#include "fgetln.h"
#include "string-utils.h"

#define strcaseequal(x, y)   (strcasecmp(x, y) == 0)

char*
get_next_arg(char *buf, char **rest)
{
	char* ret = NULL;
	char quote_type = 0;
	char* write_iter = NULL;

	// Trim whitespace
	while (isspace(*buf)) {
		buf++;
	};

	// Skip if we are empty or the start of a comment.
	if ((*buf == 0) || (*buf == '#')) {
		goto bail;
	}

	write_iter = ret = buf;

	while (*buf != 0) {
		if (quote_type != 0) {
			// We are in the middle of a quote, so we are
			// looking for matching end of the quote.
			if (*buf == quote_type) {
				quote_type = 0;
				buf++;
				continue;
			}
		} else {
			if (*buf == '"' || *buf == '\'') {
				quote_type = *buf++;
				continue;
			}

			// Stop parsing arguments if we hit unquoted whitespace.
			if (isspace(*buf)) {
				buf++;
				break;
			}
		}

		// Allow for slash-escaping
		if ((buf[0] == '\\') && (buf[1] != 0)) {
			buf++;
		}

		*write_iter++ = *buf++;
	}

	*write_iter = 0;

bail:
	if (rest) {
		*rest = buf;
	}
	return ret;
}

int
fread_config(
	FILE* file,
	config_param_set_func setter,
	void* context
) {
	int ret = 0;

	char* line = NULL;
	size_t line_len = 0;
	int line_number = 0;

	if (file == NULL) {
		ret = -1;
		goto bail;
	}

	while (!feof(file) && (line = fgetln(file, &line_len)) && !ret) {
		char *key;
		char *value;
		line_number++;

		key = get_next_arg(line, &line);
		if (!key) {
			continue;
		}

		key[strnlen(key,line_len)] = 0;

		if (strlen(key) >= line_len) {
			continue;
		}
		line_len -= strlen(key);

		value = get_next_arg(line, &line);
		if (!value) {
			continue;
		}
		value[strnlen(value, line_len)] = 0;
		ret = setter(context, key, value);
	}

bail:
	return ret;
}

int
read_config(
	const char* filename,
	config_param_set_func setter,
	void* context
) {
	int ret = 0;

	FILE* file = fopen(filename, "r");

	if (file == NULL) {
		ret = -1;
		goto bail;
	}

	syslog(LOG_INFO, "Reading configuration from \"%s\" . . .", filename);

	ret = fread_config(file, setter, context);

bail:
	if (file) {
		fclose(file);
	}
	return ret;
}

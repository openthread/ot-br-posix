/*
 *
 * Copyright (c) 2016 Nest Labs, Inc.
 * All rights reserved.
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
 *
 *    Description:
 *      This file defines utility functions for manipulating and comparing
 *      C-strings or other buffers.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "string-utils.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>

#define __USE_GNU // Needed for `strcasestr`
#include <string.h>

void
memcpyrev(void *dest_, const void *src_, size_t len)
{
	uint8_t* const dest = dest_;
	const uint8_t* const src = src_;
	int i;

	for (i = 0; i < len; i++) {
		dest[i] = src[len - 1 - i];
	}
}

int
memcmprev(const void *dest_, const void *src_, size_t len)
{
	const uint8_t* const dest = dest_;
	const uint8_t* const src = src_;
	int ret = 0;
	int i;

	for (i = 0; i < len && !ret; i++) {
		ret = dest[i] - src[len - 1 - i];
	}
	return ret;
}

void
reverse_bytes(uint8_t *bytes, size_t count)
{
	int i;

	for (i = 0; i < count / 2; i++) {
		uint8_t x = bytes[i];
		bytes[i] = bytes[count - i - 1];
		bytes[count - i - 1] = x;
	}
}

int
parse_string_into_data(uint8_t* buffer, size_t len, const char* c_str)
{
	int ret = 0;

	if (NULL == buffer) {
		len = 0;
	}

	while (*c_str != 0) {
		char c = tolower(*c_str++);
		if (!(isdigit(c) || (c >= 'a' && c <= 'f'))) {
			continue;
		}
		c = isdigit(c) ? (c - '0') : (c - 'a' + 10);
		if (len > 0) {
			*buffer = (c << 4);
			len--;
		}
		ret++;
		if (!*c_str) {
			break;
		}
		c = tolower(*c_str++);
		if (!(isdigit(c) || (c >= 'a' && c <= 'f'))) {
			continue;
		}
		c = isdigit(c) ? (c - '0') : (c - 'a' + 10);
		*buffer++ |= c;
	}

	return ret;
}

int
encode_data_into_string(
    const uint8_t*  buffer,
    size_t len,
    char*                   c_str,
    size_t c_str_max_len,
    int pad_to
    ) {
	int ret = 0;

	c_str_max_len--;
	while (len && (c_str_max_len > 4)) {
		uint8_t byte = *buffer++;
		len--;
		pad_to--;
		*c_str++ = int_to_hex_digit(byte >> 4);
		*c_str++ = int_to_hex_digit(byte & 0xF);
		c_str_max_len -= 2;
		ret += 2;
	}

	while (pad_to > 0 && (c_str_max_len > 4)) {
		pad_to--;
		*c_str++ = '0';
		*c_str++ = '0';
		c_str_max_len -= 2;
		ret += 2;
	}

	c_str_max_len--;
	*c_str++ = 0;
	return ret;
}

bool
strtobool(const char* string)
{
	bool ret = false;
	switch(string[0]) {
	case 'y':
	case 'Y':
	case 't':
	case 'T':
		ret = true;
		break;

	case 'n':
	case 'N':
	case 'f':
	case 'F':
		ret = false;
		break;

	default:
		ret = (strtol(string, NULL, 0) != 0);
		break;
	}
	return ret;
}

uint32_t
strtomask_uint32(const char* in_string)
{
	char *tmp_string = strdup(in_string);
	char *chan_ranges;      // points to a channel num or a range of channels
	char *dash_location;    // points to location of the dash in a range of channels
	uint8_t channel_start = 0;
	uint8_t channel_stop = 0;
	uint32_t mask = 0;

	chan_ranges = strtok(tmp_string, ",");
	while(chan_ranges != NULL) {
		// loop to parse channels by comma (,)
		// each fragment may include a range (-) of channels

		dash_location = strchr(chan_ranges, '-');
		if (dash_location != NULL) {
			// process a range of channels
			*dash_location = '\0';
			dash_location++;
			if (atoi(chan_ranges) < atoi(dash_location)) {
				channel_start = atoi(chan_ranges);
				channel_stop = atoi(dash_location);
			} else {
				channel_stop = atoi(chan_ranges);
				channel_start = atoi(dash_location);
			}

			while (channel_start <= channel_stop) {
				mask |= (1 << channel_start);
				channel_start++;
			}
		} else {
			// no range, just add channel to the scan mask

			mask |= (1 << strtol(chan_ranges, NULL, 0));
		}
		chan_ranges = strtok(NULL, ",");
	}
	free(tmp_string);
	return mask;
}

#include <syslog.h>

int
strtologmask(const char* value, int prev_mask)
{
	int mask = (int)strtol(value, NULL, 0);

	if (mask == 0) {
		mask = prev_mask;

		if(strcasestr(value, "all") != NULL) {
			if (strcasestr(value, "-all") != NULL) {
				mask &= 0;
			} else {
				mask |= ~0;
			}
		}
		if (strcasestr(value, "emerg") != NULL) {
			if (strcasestr(value, "-emerg") != NULL) {
				mask &= ~LOG_MASK(LOG_EMERG);
			} else {
				mask |= LOG_MASK(LOG_EMERG);
			}
		}
		if (strcasestr(value, "alert") != NULL) {
			if (strcasestr(value, "-alert") != NULL) {
				mask &= ~LOG_MASK(LOG_ALERT);
			} else {
				mask |= LOG_MASK(LOG_ALERT);
			}
		}
		if (strcasestr(value, "crit") != NULL) {
			if (strcasestr(value, "-crit") != NULL) {
				mask &= ~LOG_MASK(LOG_CRIT);
			} else {
				mask |= LOG_MASK(LOG_CRIT);
			}
		}
		if (strcasestr(value, "err") != NULL) {
			if (strcasestr(value, "-err") != NULL) {
				mask &= ~LOG_MASK(LOG_ERR);
			} else {
				mask |= LOG_MASK(LOG_ERR);
			}
		}
		if (strcasestr(value, "warn") != NULL) {
			if (strcasestr(value, "-warn") != NULL) {
				mask &= ~LOG_MASK(LOG_WARNING);
			} else {
				mask |= LOG_MASK(LOG_WARNING);
			}
		}
		if (strcasestr(value, "notice") != NULL) {
			if (strcasestr(value, "-notice") != NULL) {
				mask &= ~LOG_MASK(LOG_NOTICE);
			} else {
				mask |= LOG_MASK(LOG_NOTICE);
			}
		}
		if (strcasestr(value, "info") != NULL) {
			if (strcasestr(value, "-info") != NULL) {
				mask &= ~LOG_MASK(LOG_INFO);
			} else {
				mask |= LOG_MASK(LOG_INFO);
			}
		}
		if (strcasestr(value, "debug") != NULL) {
			if (strcasestr(value, "-debug") != NULL) {
				mask &= ~LOG_MASK(LOG_DEBUG);
			} else {
				mask |= LOG_MASK(LOG_DEBUG);
			}
		}
	}

	return mask;
}

bool
buffer_is_nonzero(const uint8_t* buffer, size_t len)
{
	while (len--) {
		if (*buffer++ != 0) {
			return true;
		}
	}
	return false;
}

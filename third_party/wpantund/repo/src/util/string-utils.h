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
 *      This file declares utility functions for manipulating and comparing
 *      C-strings or other buffers.
 *
 */

#ifndef wpantund_string_utils_h
#define wpantund_string_utils_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h>

#define strcaseequal(x, y)   (strcasecmp(x, y) == 0)
#define strncaseequal(x, y, n)   (strncasecmp(x, y, n) == 0)
#define strequal(x, y)   (strcmp(x, y) == 0)
#define strnequal(x, y, n)   (strncmp(x, y, n) == 0)

__BEGIN_DECLS
extern void memcpyrev(void* dest, const void *src, size_t len);
extern int memcmprev(const void* dest, const void *src, size_t len);
extern void reverse_bytes(uint8_t *bytes, size_t count);
extern int parse_string_into_data(uint8_t* buffer, size_t len, const char* c_str);
extern int encode_data_into_string(const uint8_t*  buffer, size_t len, char* c_str, size_t c_str_max_len, int pad_to);
extern int strtologmask(const char* value, int prev_mask);
extern bool buffer_is_nonzero(const uint8_t* buffer, size_t len);

static inline char
int_to_hex_digit(uint8_t x)
{
	return "0123456789ABCDEF"[x & 0xF];
}

uint32_t strtomask_uint32(const char* in_string);

extern bool strtobool(const char* string);

__END_DECLS

#endif

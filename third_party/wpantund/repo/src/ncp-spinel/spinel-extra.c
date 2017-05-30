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
 */

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Headers

#include "spinel-extra.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

// ----------------------------------------------------------------------------
// MARK: -

#ifndef assert_printf
#define assert_printf(fmt, ...) \
fprintf(stderr, \
        __FILE__ ":%d: " fmt "\n", \
        __LINE__, \
        __VA_ARGS__)
#endif

#ifndef require_action
#define require_action(c, l, a) \
    do { if (!(c)) { \
        assert_printf("Requirement Failed (%s)", # c); \
        a; \
        goto l; \
    } } while (0)
#endif

#ifndef require
#define require(c, l)   require_action(c, l, {})
#endif

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Datatype Iterator

spinel_status_t
spinel_datatype_iter_start(spinel_datatype_iter_t* iter, const uint8_t* data_in, spinel_size_t data_len, const char* pack_format)
{
	iter->data_ptr = data_in;
	iter->data_len = data_len;
	iter->pack_format = pack_format;

	return SPINEL_STATUS_OK;
}

spinel_status_t
spinel_datatype_iter_next(spinel_datatype_iter_t* iter)
{
	spinel_status_t ret = SPINEL_STATUS_PARSE_ERROR;
	spinel_datatype_iter_t scratchpad = *iter;
	const char* next_pack_format;

	if (!iter->data_ptr || !iter->data_len || !iter->pack_format) {
		ret = SPINEL_STATUS_EMPTY;
		goto bail;
	}

	next_pack_format = spinel_next_packed_datatype(scratchpad.pack_format);

	switch ((spinel_datatype_t)*scratchpad.pack_format) {
	case SPINEL_DATATYPE_INT8_C:
	case SPINEL_DATATYPE_UINT8_C:
		require(scratchpad.data_len >= sizeof(uint8_t), bail);
		scratchpad.data_ptr += sizeof(uint8_t);
		scratchpad.data_len -= sizeof(uint8_t);
		break;

	case SPINEL_DATATYPE_INT16_C:
	case SPINEL_DATATYPE_UINT16_C:
		require(scratchpad.data_len >= sizeof(uint16_t), bail);
		scratchpad.data_ptr += sizeof(uint16_t);
		scratchpad.data_len -= sizeof(uint16_t);
		break;

	case SPINEL_DATATYPE_INT32_C:
	case SPINEL_DATATYPE_UINT32_C:
		require(scratchpad.data_len >= sizeof(uint32_t), bail);
		scratchpad.data_ptr += sizeof(uint32_t);
		scratchpad.data_len -= sizeof(uint32_t);
		break;

	case SPINEL_DATATYPE_IPv6ADDR_C:
		require(scratchpad.data_len >= sizeof(spinel_ipv6addr_t), bail);
		scratchpad.data_ptr += sizeof(spinel_ipv6addr_t);
		scratchpad.data_len -= sizeof(spinel_ipv6addr_t);
		break;

	case SPINEL_DATATYPE_EUI64_C:
		require(scratchpad.data_len >= sizeof(spinel_eui64_t), bail);
		scratchpad.data_ptr += sizeof(spinel_eui64_t);
		scratchpad.data_len -= sizeof(spinel_eui64_t);
		break;

	case SPINEL_DATATYPE_EUI48_C:
		require(scratchpad.data_len >= sizeof(spinel_eui48_t), bail);
		scratchpad.data_ptr += sizeof(spinel_eui48_t);
		scratchpad.data_len -= sizeof(spinel_eui48_t);
		break;

	case SPINEL_DATATYPE_UINT_PACKED_C:
		{
			spinel_ssize_t pui_len = spinel_packed_uint_decode(scratchpad.data_ptr, scratchpad.data_len, NULL);
			require(pui_len > 0, bail);
			scratchpad.data_ptr += pui_len;
			scratchpad.data_len -= pui_len;
		}
		break;


	case SPINEL_DATATYPE_UTF8_C:
		{
			size_t str_len = strnlen((const char*)scratchpad.data_ptr, scratchpad.data_len) + 1;
			scratchpad.data_ptr += str_len;
			scratchpad.data_len -= str_len;
		}
		break;

	case SPINEL_DATATYPE_ARRAY_C:
	case SPINEL_DATATYPE_DATA_C:
		if ( (scratchpad.pack_format[1] == ')')
		  || (next_pack_format == 0)
		) {
			// Special case: This type is size of the rest of the buffer!
			scratchpad.data_ptr += scratchpad.data_len;
			scratchpad.data_len -= scratchpad.data_len;
			break;
		}

		// Fall through to length-prepended...
	case SPINEL_DATATYPE_STRUCT_C:
	case SPINEL_DATATYPE_DATA_WLEN_C:
		{
			spinel_ssize_t block_len = spinel_datatype_unpack(
				scratchpad.data_ptr,
				scratchpad.data_len,
				SPINEL_DATATYPE_STRUCT_S("")
			);

			require(block_len > 0, bail);
			require(block_len < SPINEL_FRAME_MAX_SIZE, bail);
			require(scratchpad.data_len >= block_len, bail);

			scratchpad.data_ptr += block_len;
			scratchpad.data_len -= block_len;

		}
		break;

	default:
		// Unsupported Type!
		goto bail;
	}

	scratchpad.pack_format = next_pack_format;

	while (scratchpad.pack_format[0] == SPINEL_DATATYPE_VOID_C) {
		scratchpad.pack_format++;
	}

	if ((*scratchpad.pack_format == ')')
	 || (*scratchpad.pack_format == 0)
	 || (scratchpad.data_len == 0)
	) {
		ret = SPINEL_STATUS_EMPTY;
	} else {
		ret = SPINEL_STATUS_OK;
	}

	if (iter->container == SPINEL_DATATYPE_ARRAY_C) {
		iter->data_ptr = scratchpad.data_ptr;
		iter->data_len = scratchpad.data_len;
	} else {
		*iter = scratchpad;
	}

bail:
	return ret;
}

spinel_status_t
spinel_datatype_iter_open_container(const spinel_datatype_iter_t* iter, spinel_datatype_iter_t* subiter)
{
	spinel_status_t ret = SPINEL_STATUS_PARSE_ERROR;
	int depth = 0;
	spinel_ssize_t block_len;

	require(iter->data_len > 2, bail);

	if ((spinel_datatype_t)*iter->pack_format == SPINEL_DATATYPE_ARRAY_C) {
		ret = SPINEL_STATUS_UNIMPLEMENTED;
		goto bail;
	}

	if ((spinel_datatype_t)*iter->pack_format != SPINEL_DATATYPE_STRUCT_C) {
		ret = SPINEL_STATUS_PARSE_ERROR;
		goto bail;
	}

	*subiter = *iter;

	require(subiter->pack_format[1] == '(', bail);

	subiter->container = iter->pack_format[0];
	subiter->pack_format += 2;

	do {
		switch(subiter->pack_format[1]) {
		case '(': depth++; break;
		case ')': depth++; break;
		case 0: depth = 0; break;
		}
	} while(depth > 0);

	require(subiter->pack_format[1] == ')', bail);
	subiter->pack_format += 2;

	block_len = spinel_datatype_unpack(
		subiter->data_ptr,
		subiter->data_len,
		SPINEL_DATATYPE_DATA_WLEN_S,
		&subiter->data_ptr,
		&subiter->data_len
	);

	require(block_len > 0, bail);

	ret = SPINEL_STATUS_OK;

bail:
	return ret;
}

spinel_status_t
spinel_datatype_iter_unpack(const spinel_datatype_iter_t* iter, ...)
{
	spinel_status_t ret = SPINEL_STATUS_PARSE_ERROR;
	va_list args;
	va_start(args, iter);
	const char pack_format[2] = { iter->pack_format[0], 0 };

	if (0 <= spinel_datatype_vunpack(iter->data_ptr, iter->data_len, pack_format, args)) {
		ret = SPINEL_STATUS_OK;
	}

	va_end(args);
	return ret;
}

spinel_status_t
spinel_datatype_iter_vunpack(const spinel_datatype_iter_t* iter, va_list args)
{
	spinel_status_t ret = SPINEL_STATUS_PARSE_ERROR;

	if (0 <= spinel_datatype_vunpack(iter->data_ptr, iter->data_len, iter->pack_format, args)) {
		ret = SPINEL_STATUS_OK;
	}

	return ret;
}

spinel_datatype_t
spinel_datatype_iter_get_type(const spinel_datatype_iter_t* iter)
{
	return iter->pack_format != NULL
		? (spinel_datatype_t)*iter->pack_format
		: SPINEL_DATATYPE_NULL_C;
}

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Command Generators

spinel_ssize_t
spinel_cmd_prop_value_set_uint(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, unsigned int x)
{
    return spinel_datatype_pack(
        cmd_data_ptr,
        cmd_data_len,
        SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT_PACKED_S),
        prop_key,
        x
    );
}

spinel_ssize_t
spinel_cmd_prop_value_set_data(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const uint8_t* x_ptr, spinel_size_t x_len)
{
    return spinel_datatype_pack(
        cmd_data_ptr,
        cmd_data_len,
        SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S),
        prop_key,
        x_ptr,
        x_len
    );
}

spinel_ssize_t
spinel_cmd_prop_value_set_utf8(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const char* x)
{
    return spinel_datatype_pack(
        cmd_data_ptr,
        cmd_data_len,
        SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S),
        prop_key,
        x
    );
}

spinel_ssize_t
spinel_cmd_prop_value_set_uint16(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, uint16_t x)
{
    return spinel_datatype_pack(
        cmd_data_ptr,
        cmd_data_len,
        SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT16_S),
        prop_key,
        x
    );
}

spinel_ssize_t
spinel_cmd_prop_value_set_ipv6addr(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const spinel_ipv6addr_t* x_ptr)
{
    return spinel_datatype_pack(
        cmd_data_ptr,
        cmd_data_len,
        SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_IPv6ADDR_S),
        prop_key,
        x_ptr
    );
}

spinel_ssize_t
spinel_cmd_prop_value_set_eui64(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const spinel_eui64_t* x_ptr)
{
    return spinel_datatype_pack(
        cmd_data_ptr,
        cmd_data_len,
        SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_EUI64_S),
        prop_key,
        x_ptr
    );
}

spinel_ssize_t
spinel_cmd_prop_value_get(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key)
{
    spinel_ssize_t ret;
    ret = spinel_datatype_pack(
        cmd_data_ptr,
        cmd_data_len,
        SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET,
        prop_key
    );
    return ret;
}

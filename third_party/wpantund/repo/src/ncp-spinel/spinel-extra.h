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

#ifndef SPINEL_EXTRA_HEADER_INCLUDED
#define SPINEL_EXTRA_HEADER_INCLUDED 1

#include "spinel.h"

__BEGIN_DECLS

// ----------------------------------------------------------------------------

typedef struct {
	const uint8_t* data_ptr;
	int data_len;
	const char* pack_format;
	spinel_datatype_t container;
} spinel_datatype_iter_t;

SPINEL_API_EXTERN spinel_status_t spinel_datatype_iter_start(spinel_datatype_iter_t* iter, const uint8_t* data_in, spinel_size_t data_len, const char* pack_format);
SPINEL_API_EXTERN spinel_status_t spinel_datatype_iter_next(spinel_datatype_iter_t* iter);
SPINEL_API_EXTERN spinel_status_t spinel_datatype_iter_open_container(const spinel_datatype_iter_t* iter, spinel_datatype_iter_t* subiter);
SPINEL_API_EXTERN spinel_status_t spinel_datatype_iter_unpack(const spinel_datatype_iter_t* iter, ...);
SPINEL_API_EXTERN spinel_status_t spinel_datatype_iter_vunpack(const spinel_datatype_iter_t* iter, va_list args);
SPINEL_API_EXTERN spinel_datatype_t spinel_datatype_iter_get_type(const spinel_datatype_iter_t* iter);

// ----------------------------------------------------------------------------

#define SPINEL_FRAME_PACK_CMD(x)                     "Ci" x,SPINEL_HEADER_FLAG
#define SPINEL_FRAME_PACK_CMD_NOOP                   SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_NULL_S),SPINEL_CMD_NOOP
#define SPINEL_FRAME_PACK_CMD_RESET                  SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_NULL_S),SPINEL_CMD_RESET
#define SPINEL_FRAME_PACK_CMD_NET_CLEAR              SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_NULL_S),SPINEL_CMD_NET_CLEAR
#define SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET         SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S),SPINEL_CMD_PROP_VALUE_GET
#define SPINEL_FRAME_PACK_CMD_PROP_TYPE_GET          SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S),SPINEL_CMD_PROP_TYPE_GET
#define SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(x)      SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S x),SPINEL_CMD_PROP_VALUE_SET
#define SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(x)   SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S x),SPINEL_CMD_PROP_VALUE_INSERT
#define SPINEL_FRAME_PACK_CMD_PROP_VALUE_REMOVE(x)   SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S x),SPINEL_CMD_PROP_VALUE_REMOVE
#define SPINEL_FRAME_PACK_CMD_PROP_VALUE_IS(x)       SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S x),SPINEL_CMD_PROP_VALUE_IS
#define SPINEL_FRAME_PACK_CMD_PROP_TYPE_IS           SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S SPINEL_DATATYPE_UINT8_S),SPINEL_CMD_PROP_TYPE_IS
#define SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERTED(x) SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S x),SPINEL_CMD_PROP_VALUE_INSERTED
#define SPINEL_FRAME_PACK_CMD_PROP_VALUE_REMOVED(x)  SPINEL_FRAME_PACK_CMD(SPINEL_DATATYPE_UINT_PACKED_S x),SPINEL_CMD_PROP_VALUE_REMOVED

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_value_set_data(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const uint8_t* x_ptr, spinel_size_t x_len);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_value_set_utf8(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const char* x);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_value_set_uint(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, unsigned int x);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_value_set_uint16(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, uint16_t x);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_value_set_ipv6addr(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const spinel_ipv6addr_t* x_ptr);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_value_set_eui64(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key, const spinel_eui64_t* x_ptr);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_value_get(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_type_get(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key);

SPINEL_API_EXTERN spinel_ssize_t spinel_cmd_prop_type_get(uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len, spinel_prop_key_t prop_key);

__END_DECLS

#endif // SPINEL_EXTRA_HEADER_INCLUDED

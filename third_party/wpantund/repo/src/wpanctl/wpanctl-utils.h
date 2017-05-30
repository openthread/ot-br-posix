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

#ifndef WPANCTL_UTILS_H
#define WPANCTL_UTILS_H

#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "wpan-error.h"

#define ERRORCODE_OK            (0)
#define ERRORCODE_HELP          (1)
#define ERRORCODE_BADARG        (2)
#define ERRORCODE_NOCOMMAND     (3)
#define ERRORCODE_UNKNOWN       (4)
#define ERRORCODE_BADCOMMAND    (5)
#define ERRORCODE_NOREADLINE    (6)
#define ERRORCODE_QUIT          (7)
#define ERRORCODE_BADCONFIG     (8)
#define ERRORCODE_ERRNO         (9)
#define ERRORCODE_NOT_IMPLEMENTED           (10)
#define ERRORCODE_TIMEOUT           (11)
#define ERRORCODE_BADVERSION     (12)
#define ERRORCODE_ALLOC           (13)
#define ERRORCODE_NOTFOUND     (14)
#define ERRORCODE_REFUSED     (15)

#define ERRORCODE_INTERRUPT     (128 + SIGINT)
#define ERRORCODE_SIGHUP        (128 + SIGHUP)

#define DEFAULT_TIMEOUT_IN_SECONDS      60

struct command_info_s {
	const char* name;
	const char* desc;
	int (*entrypoint)(
	    int argc, char* argv[]);
	int isHidden;
};

struct wpan_network_info_s {
	char network_name[17];
	dbus_bool_t allowing_join;
	uint16_t pan_id;
	int16_t channel;
	uint64_t xpanid;
	int8_t rssi;
	uint8_t type;
	uint8_t hwaddr[8];
};

void print_error_diagnosis(int error);
int parse_network_info_from_iter(struct wpan_network_info_s *network_info, DBusMessageIter *iter);
int parse_energy_scan_result_from_iter(int16_t *channel, int8_t *maxRssi, DBusMessageIter *iter);
int lookup_dbus_name_from_interface(char* dbus_bus_name, const char* interface_name);
void dump_info_from_iter(FILE* file, DBusMessageIter *iter, int indent, bool bare, bool indentFirstLine);
uint16_t node_type_str2int(const char *node_type);
const char *node_type_int2str(uint16_t node_type);

extern char gInterfaceName[32];
extern int gRet;

#endif

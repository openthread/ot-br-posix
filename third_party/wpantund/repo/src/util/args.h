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

#ifndef UTIL_ARGS_HEADER_INCLUDED
#define UTIL_ARGS_HEADER_INCLUDED 1

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
	char shortarg;
	const char* longarg;
	const char* param;
	const char* desc;
} arg_list_item_t;

static inline void
print_arg_list_help(
    const arg_list_item_t arg_list[],
    const char*                             command_name,
    const char*                             syntax
    )
{
	int i;

	printf("Syntax:\n");
	printf("   %s %s\n", command_name, syntax);
	printf("Options:\n");
	for (i = 0; arg_list[i].desc; ++i) {
		if (arg_list[i].shortarg)
			printf("   -%c", arg_list[i].shortarg);
		else
			printf("     ");

		if (arg_list[i].longarg) {
			if (arg_list[i].shortarg)
				printf("/");
			else
				printf(" ");

			printf("--%s%s",
			       arg_list[i].longarg,
			       &"                    "[strlen(arg_list[i].longarg)]);
		} else {
			printf("                       ");
		}

		if (arg_list[i].param != NULL) {
			printf(" %s [%s]\n", arg_list[i].desc, arg_list[i].param);
		} else {
			printf(" %s\n", arg_list[i].desc);
		}

	}
}

#endif // UTIL_ARGS_HEADER_INCLUDED

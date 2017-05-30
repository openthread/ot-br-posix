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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "wpanctl-utils.h"
#include "tool-cmd-cd.h"
#include <string.h>

int tool_cmd_cd(int argc, char* argv[])
{
	int ret = 0;

	if ((2 == argc) && (0 == strcmp(argv[1], "--help"))) {
		fprintf(stderr,
		        "%s: Help not yet implemented for this command.\n",
		        argv[0]);
		ret = ERRORCODE_HELP;
	}

	if (argc == 1) {
		printf("%s\n", gInterfaceName);
		ret = ERRORCODE_HELP;
	} else if (argc == 2) {
		strncpy(gInterfaceName, argv[1], sizeof(gInterfaceName) - 1);
	}

bail:
	return ret;
}

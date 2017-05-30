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

#include <stdio.h>
#include <stdlib.h>
#include "sec-random.h"

static FILE* gSecRandomFile;

int
sec_random_init(void)
{
	if (gSecRandomFile == NULL) {
		const char* random_source_filename = getenv("SEC_RANDOM_SOURCE_FILE");

		if (!random_source_filename) {
			random_source_filename = "/dev/urandom";
		}

		gSecRandomFile = fopen(random_source_filename, "r");

		if (gSecRandomFile == NULL) {
			return -1;
		}
	}
	return 0;
}

int
sec_random_fill(uint8_t* buffer, int length)
{
	int ret = sec_random_init();

	if (ret >= 0) {
		ret = (int)fread(buffer, length, 1, gSecRandomFile);
	}

	return ret;
}

/*
 * Copyright (c) 2017 Nest Labs, Inc.
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
 *		This file implements the wpantund fuzzer.
 *
 */

#define main __XX_main
#include "wpantund.cpp"


int
ConfigFileFuzzTarget(const uint8_t *data, size_t size) {
	std::map<std::string, std::string> settings;
	FILE *file = tmpfile();
	fwrite(data, 1, size, file);
	fflush(file);
	rewind(file);
	fread_config(file, &add_to_map, &settings);
	if (!settings.empty()) {
		std::map<std::string, std::string>::const_iterator iter;

		for(iter = settings.begin(); iter != settings.end(); iter++) {
			set_config_param(NULL, iter->first.c_str(), iter->second.c_str());
		}

		MainLoop main_loop(settings);
	}
	fclose(file);

	return 0;
}

int
NCPInputFuzzTarget(const uint8_t *data, size_t size) {
#if 0 // Not working yet
	std::map<std::string, std::string> settings;
	char filename[L_tmpnam];

	FILE *file = fopen(tmpnam(filename), "w+");

	settings[kWPANTUNDProperty_ConfigNCPSocketPath] = filename;

	fwrite(data, 1, size, file);
	fflush(file);
	rewind(file);

	MainLoop main_loop(settings);

	main_loop.process();

	for (;size > 0; size--,data++) {
		// TODO: Feed data, one byte at a time.

		main_loop.process();
		main_loop.process();
	}

	main_loop.process();

	fclose(file);
	unlink(filename);
#endif
	return 0;
}

int
NCPControlInterfaceFuzzTarget(const uint8_t *data, size_t size) {
	// TODO: Write me!
	return 0;
}


extern "C" int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static bool did_init;

	if (!did_init) {
		did_init = true;

		//openlog("wpantund-fuzz", LOG_PERROR | LOG_PID | LOG_CONS, LOG_DAEMON);
	}

	if (size >= 1) {
		char type = *data++;
		size--;
		switch (type) {
		case '0': // Config file fuzzing
			return ConfigFileFuzzTarget(data, size);
			break;

		case '1': // NCP Input fuzzing
			return NCPInputFuzzTarget(data, size);
			break;

		case '2': // NCPControlInterface fuzzing
			return NCPControlInterfaceFuzzTarget(data, size);
			break;

		default:
			break;
		}
	}

	return 0;
}

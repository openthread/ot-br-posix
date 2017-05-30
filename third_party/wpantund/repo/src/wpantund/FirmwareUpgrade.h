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
 *      Firmware Upgrade Manager
 *
 */

#ifndef __wpantund__FirmwareUpgrade__
#define __wpantund__FirmwareUpgrade__

#include <stdio.h>
#include <sys/select.h>
#include <string>
#include "time-utils.h"

namespace nl {
namespace wpantund {

class FirmwareUpgrade {
public:
	FirmwareUpgrade();
	~FirmwareUpgrade();

	bool is_firmware_upgrade_required(const std::string& version);
	void upgrade_firmware(void);

	void set_firmware_upgrade_command(const std::string& command);
	void set_firmware_check_command(const std::string& command);

	// May return:
	//  * `0`: An upgrade was not started or the upgrade completed successfully.
	//	* `EINPROGRESS`: An upgrade is currently in process.
	//  * Any Other Value: An error occured when attempting to upgrade.
	int get_upgrade_status(void);

	int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout);
	void process(void);

	bool can_upgrade_firmware(void);

private:
	void close_check_fd(void);
	void close_upgrade_fd(void);

private:
	int mUpgradeStatus;
	int mFirmwareCheckFD;
	int mFirmwareUpgradeFD;
};

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__FirmwareUpgrade__) */

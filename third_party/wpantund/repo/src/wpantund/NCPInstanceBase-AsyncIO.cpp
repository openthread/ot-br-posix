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

#include "assert-macros.h"
#include "NCPInstanceBase.h"
#include "tunnel.h"
#include <syslog.h>
#include <errno.h>
#include "nlpt.h"
#include <algorithm>
#include "socket-utils.h"
#include "SuperSocket.h"

using namespace nl;
using namespace wpantund;

bool
NCPInstanceBase::can_set_ncp_power(void)
{
	return mPowerFD >= 0;
}

int
NCPInstanceBase::set_ncp_power(bool power)
{
	ssize_t ret = -1;

	if (mPowerFD >= 0) {
		// Since controlling the power such a low-level
		// operation, we break with our usual "no blocking calls"
		// rule here for code clarity and to avoid
		// unnecessary complexity.

		IGNORE_RETURN_VALUE( lseek(mPowerFD, 0, SEEK_SET));

		if (power) {
			ret = write(mPowerFD, static_cast<const void*>(&mPowerFD_PowerOn), 1);
		} else {
			ret = write(mPowerFD, static_cast<const void*>(&mPowerFD_PowerOff), 1);
		}

		require_string(ret >= 0, bail, strerror(errno));

		// We assign the return value of `write()` here
		// to `ret` because we don't care if writing the `\n`
		// fails. This happens when writing directly to GPIO
		// files. We write the "\n" anyway to make it easier
		// for non-GPIO sockets to parse.
		IGNORE_RETURN_VALUE( write(mPowerFD, static_cast<const void*>("\n"), 1) );
	}

	if (ret > 0) {
		ret = 0;
	}

bail:

	return static_cast<int>(ret);
}

void
NCPInstanceBase::hard_reset_ncp(void)
{
	NLPT_INIT(&mDriverToNCPPumpPT);
	NLPT_INIT(&mNCPToDriverPumpPT);

	if (mResetFD >= 0) {
		// Since hardware resets are such a low-level
		// operation, we break with our usual "no blocking calls"
		// rule here for code clarity and to avoid
		// unnecessary complexity.

		ssize_t wret = -1;

		IGNORE_RETURN_VALUE( lseek(mResetFD, 0, SEEK_SET) );

		wret = write(mResetFD, static_cast<const void*>(&mResetFD_BeginReset), 1);

		check_string(wret != -1, strerror(errno));

		// We don't assign the return value of `write()` here
		// to `ret` because we don't care if writing the `\n`
		// fails. This happens when writing directly to GPIO
		// files. We write the "\n" anyway to make it easier
		// for non-GPIO sockets to parse.
		IGNORE_RETURN_VALUE( write(mResetFD, static_cast<const void*>("\n"), 1) );

		usleep(20 * USEC_PER_MSEC);

		IGNORE_RETURN_VALUE( lseek(mResetFD, 0, SEEK_SET) );

		wret = write(mResetFD, static_cast<const void*>(&mResetFD_EndReset), 1);

		check_string(wret != -1, strerror(errno));

		IGNORE_RETURN_VALUE( write(mResetFD, static_cast<const void*>("\n"), 1) );
	} else {
		mSerialAdapter->reset();
	}
}

void
NCPInstanceBase::set_socket_adapter(const boost::shared_ptr<SocketAdapter> &adapter)
{
	if(adapter) {
		adapter->set_parent(mRawSerialAdapter);
		mSerialAdapter = adapter;
	} else {
		mSerialAdapter = mRawSerialAdapter;
	}
}

// ----------------------------------------------------------------------------
// MARK: -

cms_t
NCPInstanceBase::get_ms_to_next_event(void)
{
	cms_t ret(EventHandler::get_ms_to_next_event());

	mSerialAdapter->update_fd_set(NULL, NULL, NULL, NULL, &ret);
	mPrimaryInterface->update_fd_set(NULL, NULL, NULL, NULL, &ret);
	mFirmwareUpgrade.update_fd_set(NULL, NULL, NULL, NULL, &ret);

	if (mWasBusy && (mLastChangedBusy != 0)) {
		cms_t temp_cms(MAX_INSOMNIA_TIME_IN_MS - (time_ms() - mLastChangedBusy));
		if (temp_cms < ret) {
			ret = temp_cms;
		}

		if (ret > BUSY_DEBOUNCE_TIME_IN_MS && !is_busy()) {
			ret = BUSY_DEBOUNCE_TIME_IN_MS;
		}
	}

	if (ret < 0) {
		ret = 0;
	}

	return ret;
}

int
NCPInstanceBase::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	int ret = -1;

	if (timeout != NULL) {
		*timeout = std::min(*timeout, get_ms_to_next_event());
	}

	ret = mFirmwareUpgrade.update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout);

	require_noerr(ret, bail);

	ret = mPcapManager.update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout);

	require_noerr(ret, bail);

	if (!ncp_state_is_detached_from_ncp(get_ncp_state())) {
		nlpt_select_update_fd_set(&mDriverToNCPPumpPT, read_fd_set, write_fd_set, error_fd_set, max_fd);
		nlpt_select_update_fd_set(&mNCPToDriverPumpPT, read_fd_set, write_fd_set, error_fd_set, max_fd);

		ret = mPrimaryInterface->update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout);
		require_noerr(ret, bail);

		if (is_legacy_interface_enabled()) {
			ret = mLegacyInterface->update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout);
			require_noerr(ret, bail);
		}

		ret = mSerialAdapter->update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout);
		require_noerr(ret, bail);
	}

bail:
	return ret;
}

void
NCPInstanceBase::process(void)
{
	int ret = 0;

	mRunawayResetBackoffManager.update();

	mFirmwareUpgrade.process();

	mPcapManager.process();

	if (get_upgrade_status() != EINPROGRESS) {
		refresh_global_addresses();

		require_noerr(ret = mPrimaryInterface->process(), socket_failure);

		if (is_legacy_interface_enabled()) {
			mLegacyInterface->process();
		}

		require_noerr(ret = mSerialAdapter->process(), socket_failure);

		ncp_to_driver_pump();
	}

	EventHandler::process_event(EVENT_IDLE);

	if (get_upgrade_status() != EINPROGRESS) {
		driver_to_ncp_pump();
	}

    update_busy_indication();

	return;

socket_failure:
	signal_fatal_error(ret);
	return;
}

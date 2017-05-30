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

#include "DummyNCPInstance.h"
#include "time-utils.h"
#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "socket-utils.h"
#include <stdexcept>
#include <sys/file.h>
#include "SuperSocket.h"

using namespace nl;
using namespace wpantund;

WPANTUND_DEFINE_NCPINSTANCE_PLUGIN(dummy, DummyNCPInstance);

DummyNCPInstance::DummyNCPInstance(const Settings& settings) :
	NCPInstanceBase(settings), mControlInterface(this)
{
}

DummyNCPInstance::~DummyNCPInstance()
{
}

int
DummyNCPInstance::vprocess_event(int event, va_list args)
{
	EH_BEGIN();

	EH_SLEEP_FOR(1);

	change_ncp_state(OFFLINE);
	signal_property_changed(kWPANTUNDProperty_NCPState, ncp_state_to_string(get_ncp_state()));

	// Wait forever, this is the dummy plugin.
	EH_WAIT_UNTIL(false);

	EH_END();
}

char
DummyNCPInstance::ncp_to_driver_pump()
{
	struct nlpt*const pt = &mNCPToDriverPumpPT;

	NLPT_BEGIN(pt);
	NLPT_END(pt);
}

char
DummyNCPInstance::driver_to_ncp_pump()
{
	struct nlpt*const pt = &mNCPToDriverPumpPT;

	NLPT_BEGIN(pt);
	NLPT_END(pt);
}

bool
DummyNCPInstance::setup_property_supported_by_class(const std::string& prop_name)
{
	return NCPInstanceBase::setup_property_supported_by_class(prop_name);
}

DummyNCPControlInterface&
DummyNCPInstance::get_control_interface()
{
	return mControlInterface;
}

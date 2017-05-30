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

#ifndef __wpantund__DummyNCPInstance__
#define __wpantund__DummyNCPInstance__

#include "NCPInstanceBase.h"
#include "DummyNCPControlInterface.h"
#include "nlpt.h"
#include "SocketWrapper.h"
#include "SocketAsyncOp.h"

#include <queue>
#include <set>
#include <map>
#include <errno.h>

WPANTUND_DECLARE_NCPINSTANCE_PLUGIN(dummy, DummyNCPInstance);

namespace nl {
namespace wpantund {

class DummyNCPControlInterface;

class DummyNCPInstance : public NCPInstanceBase {
	friend class DummyNCPControlInterface;

public:
	DummyNCPInstance(const Settings& settings = Settings());

	virtual ~DummyNCPInstance();

	virtual DummyNCPControlInterface& get_control_interface();

	virtual int vprocess_event(int event, va_list args);

protected:
	virtual char ncp_to_driver_pump();
	virtual char driver_to_ncp_pump();


public:
	static bool setup_property_supported_by_class(const std::string& prop_name);

private:
	DummyNCPControlInterface mControlInterface;
}; // class DummyNCPInstance

extern class DummyNCPInstance* gNCPInstance;

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__DummyNCPInstance__) */
